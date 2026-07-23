// In-RAM asset store. The Flipper streams the (gzipped) web bundle over UART at
// session start; we hold each file in a heap buffer and serve it from the HTTP
// catch-all. No filesystem, so nothing survives a reboot (the Flipper re-streams).
#pragma once
#include <Arduino.h>

#define HA_MAX_ASSETS 8
#define HA_ASSET_PATH 40
#define HA_ASSET_MIME 32

struct Asset {
    char path[HA_ASSET_PATH];
    char mime[HA_ASSET_MIME];
    bool gzip;
    uint8_t* buf;
    size_t len; // final length once fully received
};

class AssetStore {
public:
    void clear() {
        for(int i = 0; i < _count; i++) {
            if(_items[i].buf) free(_items[i].buf);
            _items[i].buf = nullptr;
        }
        _count = 0;
        _cur = -1;
        _need = 0;
    }

    // Start receiving a new file. `total` raw bytes will follow via feed().
    bool begin(const char* path, const char* mime, bool gzip, size_t total) {
        if(_count >= HA_MAX_ASSETS) return false;
        Asset& a = _items[_count];
        strlcpy(a.path, path, sizeof(a.path));
        strlcpy(a.mime, mime[0] ? mime : "application/octet-stream", sizeof(a.mime));
        a.gzip = gzip;
        a.len = 0;
        a.buf = (uint8_t*)malloc(total ? total : 1);
        if(!a.buf) return false;
        _cur = _count;
        _got = 0;
        _need = total;
        _count++;
        if(total == 0) commitCur();
        return true;
    }

    // Append streamed bytes to the file being received. Returns bytes consumed.
    size_t feed(const uint8_t* d, size_t n) {
        if(_cur < 0) return 0;
        size_t k = n < _need ? n : _need;
        memcpy(_items[_cur].buf + _got, d, k);
        _got += k;
        _need -= k;
        if(_need == 0) commitCur();
        return k;
    }

    bool receiving() const { return _cur >= 0; }
    size_t remaining() const { return _need; }
    int count() const { return _count; }

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
    void commitCur() {
        _items[_cur].len = _got;
        _cur = -1;
    }

    Asset _items[HA_MAX_ASSETS] = {};
    int _count = 0;
    int _cur = -1;
    size_t _got = 0;
    size_t _need = 0;
};
