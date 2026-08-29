// Flash-backed asset store. The Flipper streams the (gzipped) web bundle over UART at
// session start; we persist each file in a LittleFS flash partition and serve it from the
// HTTP catch-all. Unlike the old RAM store this survives a reboot and costs ~no heap, so
// the AP no longer runs out of RAM when phones associate. The Flipper skips re-streaming
// when its bundle CRC already matches the one we advertise in the PING beacon.
//
// Bytes are buffered in a transient RAM buffer as they stream, then flushed to LittleFS at
// commit -- NOT written to flash mid-stream. A flash write stalls the CPU and disables the
// cache, which blocks the (non-IRAM) UART ISR and overflows the RX FIFO at 921600 baud; the
// flush therefore runs in the idle gap after the last byte, while the Flipper waits for
// "fok". The RAM buffer is freed immediately after, so nothing large stays resident.
#pragma once
#include <Arduino.h>
#include <LittleFS.h>

#define HA_MAX_ASSETS 8
#define HA_ASSET_PATH 40
#define HA_ASSET_MIME 32
#define HA_ASSET_FS 16
#define HA_BUNDLE_META "/bundle.meta"

// CRC-32/ISO-HDLC (zlib/IEEE): poly 0xEDB88320 reflected, init/xorout 0xFFFFFFFF. Kept
// byte-identical to web/build.mjs -- this is the bundle identity the Flipper compares
// against our PING-advertised CRC to decide whether to re-stream. `state` is the running
// internal value (start at 0xFFFFFFFF); finalize with ^0xFFFFFFFF.
static inline uint32_t ha_crc32_run(uint32_t state, const uint8_t* d, size_t n) {
    for(size_t i = 0; i < n; i++) {
        state ^= d[i];
        for(int b = 0; b < 8; b++)
            state = (state & 1) ? (state >> 1) ^ 0xEDB88320u : (state >> 1);
    }
    return state;
}

struct Asset {
    char path[HA_ASSET_PATH];
    char mime[HA_ASSET_MIME];
    char fsname[HA_ASSET_FS]; // LittleFS filename, e.g. "/b0.gz"
    bool gzip;
    uint32_t len; // final length once fully received
};

class AssetStore {
public:
    // Load a previously stored bundle from flash. Call once in setup() after LittleFS has
    // mounted. Verifies the stored files against the recorded CRC so a stream that died
    // mid-write is not trusted (falls back to "no bundle" -> the Flipper re-streams).
    void load() {
        _count = 0;
        _cur = -1;
        _need = 0;
        _dirty = false;
        _failed = false;
        _bundleCrc = 0;
        File m = LittleFS.open(HA_BUNDLE_META, "r");
        if(!m) return;
        uint8_t cnt = 0;
        uint32_t crc = 0;
        bool ok = (m.read(&cnt, 1) == 1) && (m.read((uint8_t*)&crc, 4) == 4) && cnt <= HA_MAX_ASSETS;
        for(uint8_t i = 0; ok && i < cnt; i++)
            ok = m.read((uint8_t*)&_items[i], sizeof(Asset)) == (int)sizeof(Asset);
        m.close();
        if(!ok) return;
        uint32_t v = 0xFFFFFFFFu;
        for(uint8_t i = 0; i < cnt; i++) {
            File f = LittleFS.open(_items[i].fsname, "r");
            if(!f || (uint32_t)f.size() != _items[i].len) {
                if(f) f.close();
                return;
            }
            uint8_t buf[256];
            int r;
            while((r = f.read(buf, sizeof(buf))) > 0) v = ha_crc32_run(v, buf, (size_t)r);
            f.close();
        }
        if((v ^ 0xFFFFFFFFu) != crc) return; // corrupt/partial -> treat as no bundle
        _count = cnt;
        _bundleCrc = crc;
    }

    // CLEAR_FILES: start a fresh receive round -- wipe the committed bundle. Only sent by
    // the Flipper when it is going to stream (a skip never sends CLEAR_FILES), so this
    // marks a real update.
    void clear() {
        for(int i = 0; i < _count; i++) LittleFS.remove(_items[i].fsname);
        _count = 0;
        _cur = -1;
        _need = 0;
        _dirty = true;
        _failed = false;
        _run = 0xFFFFFFFFu;
        freeBuf();
    }

    // Start receiving a file. Allocates a transient RAM buffer for its `total` bytes (no
    // flash yet). On OOM or over the asset cap it still drains the stream but stores
    // nothing (so the protocol stays in sync and the Flipper re-streams next time).
    bool begin(const char* path, const char* mime, bool gzip, size_t total) {
        freeBuf();
        int idx = (_count < HA_MAX_ASSETS) ? _count : -1;
        if(idx >= 0) {
            Asset& a = _items[idx];
            strlcpy(a.path, path, sizeof(a.path));
            strlcpy(a.mime, mime[0] ? mime : "application/octet-stream", sizeof(a.mime));
            a.gzip = gzip;
            a.len = 0;
            // The ".gz" suffix is what makes AsyncFileResponse add Content-Encoding: gzip,
            // so only name gzipped assets that way.
            snprintf(a.fsname, sizeof(a.fsname), gzip ? "/b%d.gz" : "/b%d", idx);
            _buf = (uint8_t*)malloc(total ? total : 1);
        }
        if(idx < 0 || !_buf) _failed = true;
        _cur = idx;
        _got = 0;
        _need = total;
        if(idx >= 0) _count++;
        if(total == 0) commitCur();
        return true;
    }

    // Buffer streamed bytes in RAM (no flash here -- see the file header). Always counts
    // down `_need` so the raw-drain terminates even when we're discarding (OOM/over cap).
    size_t feed(const uint8_t* d, size_t n) {
        size_t k = n < _need ? n : _need;
        if(_buf && _cur >= 0) memcpy(_buf + _got, d, k);
        _run = ha_crc32_run(_run, d, k);
        _got += k;
        _need -= k;
        if(_need == 0) commitCur();
        return k;
    }

    // Called when the Flipper has finished streaming (on SET_AP). If a real stream round
    // happened, finalize the bundle CRC and persist the metadata so both the bundle and its
    // identity survive a reboot. A skipped session, or one where a file failed to store,
    // leaves/advertises no CRC so the Flipper re-streams.
    void finishStream() {
        if(!_dirty) return;
        _dirty = false;
        if(_failed) {
            _bundleCrc = 0;
            return;
        }
        _bundleCrc = _run ^ 0xFFFFFFFFu;
        File m = LittleFS.open(HA_BUNDLE_META, "w");
        if(!m) {
            _bundleCrc = 0; // couldn't persist -> advertise nothing so the Flipper re-streams
            return;
        }
        uint8_t cnt = (uint8_t)_count;
        m.write(&cnt, 1);
        m.write((const uint8_t*)&_bundleCrc, 4);
        for(int i = 0; i < _count; i++) m.write((const uint8_t*)&_items[i], sizeof(Asset));
        m.close();
    }

    bool receiving() const { return _need > 0; }
    size_t remaining() const { return _need; }
    int count() const { return _count; }
    uint32_t bundleCrc() const { return _bundleCrc; }

    const Asset* find(const char* path) const {
        for(int i = 0; i < _count; i++) {
            if(strcmp(_items[i].path, path) == 0) return &_items[i];
        }
        return nullptr;
    }

    // The file served for "/" (and thus for every captive-detection URL).
    const Asset* root() const {
        const Asset* r = find("/");
        if(r) return r;
        return _count > 0 ? &_items[0] : nullptr;
    }

private:
    // Flush the just-received file from RAM to flash, then free the buffer. Runs when
    // `_need` hits 0 (all bytes in) -- i.e. while the UART is idle waiting for "fok".
    void commitCur() {
        if(_cur >= 0) {
            File f = _buf ? LittleFS.open(_items[_cur].fsname, "w") : File();
            if(_buf && f && (size_t)f.write(_buf, _got) == _got) {
                f.close();
                _items[_cur].len = _got;
            } else {
                if(f) f.close();
                _failed = true;
            }
        }
        freeBuf();
        _cur = -1;
    }

    void freeBuf() {
        if(_buf) {
            free(_buf);
            _buf = nullptr;
        }
    }

    Asset _items[HA_MAX_ASSETS] = {};
    uint8_t* _buf = nullptr; // transient buffer for the file being received (freed at commit)
    int _count = 0;
    int _cur = -1;
    size_t _got = 0;
    size_t _need = 0;
    bool _dirty = false;
    bool _failed = false;          // a file this round couldn't be stored (OOM / write error)
    uint32_t _run = 0xFFFFFFFFu;   // running CRC over the bytes of the current stream round
    uint32_t _bundleCrc = 0;       // finalized, persisted CRC of the stored bundle (0 = none)
};
