#include "onewire_reader.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_resources.h>
#include <one_wire/one_wire_host.h>
#include <one_wire/maxim_crc.h>
#include <string.h>

#define ONEWIRE_CMD_READ_ROM 0x33
#define POLL_MS 60u

struct OneWireReader {
    OneWireHost* host;
    FuriThread* thread;
    FuriMutex* mutex;

    volatile bool stop;
    OneWireReaderState state;
    OneWireReading result;
};

static void lock(OneWireReader* r) {
    furi_mutex_acquire(r->mutex, FuriWaitForever);
}
static void unlock(OneWireReader* r) {
    furi_mutex_release(r->mutex);
}

/* One attempt: returns true and fills `rom` if a key answered with 8 bytes. */
static bool onewire_read_once(OneWireReader* r, uint8_t* rom) {
    bool ok = false;
    onewire_host_start(r->host);
    FURI_CRITICAL_ENTER();
    if(onewire_host_reset(r->host)) {
        onewire_host_write(r->host, ONEWIRE_CMD_READ_ROM);
        onewire_host_read_bytes(r->host, rom, 8);
        ok = true;
    }
    FURI_CRITICAL_EXIT();
    onewire_host_stop(r->host);

    /* A missing key clocks in all-zero or all-ones; reject those. */
    if(ok) {
        uint8_t acc_and = 0xFF, acc_or = 0x00;
        for(int i = 0; i < 8; i++) {
            acc_and &= rom[i];
            acc_or |= rom[i];
        }
        if(acc_or == 0x00 || acc_and == 0xFF) ok = false;
    }
    return ok;
}

static int32_t onewire_worker(void* context) {
    OneWireReader* r = context;

    r->host = onewire_host_alloc(&gpio_ibutton);

    lock(r);
    r->state = OneWireScanning;
    unlock(r);

    uint8_t rom[8];
    while(!r->stop) {
        if(onewire_read_once(r, rom)) {
            OneWireReading res;
            memset(&res, 0, sizeof(res));
            memcpy(res.rom, rom, 8);
            res.crc_calc = maxim_crc8(rom, 7, MAXIM_CRC8_INIT);
            res.crc_ok = (res.crc_calc == rom[7]);

            lock(r);
            r->result = res;
            r->state = OneWireReady;
            unlock(r);
            break;
        }
        furi_delay_ms(POLL_MS);
    }

    onewire_host_free(r->host);
    r->host = NULL;
    return 0;
}

/* -------------------------------------------------------------- lifecycle */

OneWireReader* onewire_reader_alloc(void) {
    OneWireReader* r = malloc(sizeof(OneWireReader));
    memset(r, 0, sizeof(OneWireReader));
    r->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    r->state = OneWireIdle;
    return r;
}

void onewire_reader_free(OneWireReader* r) {
    furi_assert(r);
    onewire_reader_stop(r);
    furi_mutex_free(r->mutex);
    free(r);
}

void onewire_reader_start(OneWireReader* r) {
    furi_assert(r);
    onewire_reader_stop(r);

    r->stop = false;
    lock(r);
    r->state = OneWireScanning;
    unlock(r);

    r->thread = furi_thread_alloc_ex("RosettaOneWire", 2 * 1024, onewire_worker, r);
    furi_thread_start(r->thread);
}

void onewire_reader_stop(OneWireReader* r) {
    furi_assert(r);
    r->stop = true;
    if(r->thread) {
        furi_thread_join(r->thread);
        furi_thread_free(r->thread);
        r->thread = NULL;
    }
    lock(r);
    if(r->state != OneWireReady) r->state = OneWireIdle;
    unlock(r);
}

OneWireReaderState onewire_reader_state(OneWireReader* r) {
    furi_assert(r);
    lock(r);
    OneWireReaderState s = r->state;
    unlock(r);
    return s;
}

bool onewire_reader_get(OneWireReader* r, OneWireReading* out) {
    furi_assert(r);
    furi_assert(out);
    lock(r);
    bool ready = (r->state == OneWireReady);
    if(ready) *out = r->result;
    unlock(r);
    return ready;
}
