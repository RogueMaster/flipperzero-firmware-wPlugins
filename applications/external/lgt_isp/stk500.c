/* ============================================================
 * stk500.c  —  STK500v1-Slave, uebersetzt auf LGT-SWD.
 * avrdude (-c stk500v1 -p m328p) redet mit dem Flipper wie mit einem 328P-
 * Programmer; die Flash-Kommandos werden auf LGT-EEE gemappt. Die gemeldete
 * Signatur ist 1E 95 0F (328P) — der LGT8F328P ist flash-kompatibel.
 *
 * Ablauf einer avrdude-Session (ein -U flash:w): ENTER_PROGMODE (= LGT Unlock+
 * Erase, EINMAL) -> READ_SIGN -> je Seite LOAD_ADDRESS + PROG_PAGE -> Verify
 * per READ_PAGE (gleiche Session, liest das eben Geschriebene) -> LEAVE.
 * ============================================================ */
#include "stk500.h"
#include "lgt_swd.h"

/* Antworten */
#define STK_OK             0x10
#define STK_INSYNC         0x14
#define STK_NOSYNC         0x15
#define CRC_EOP            0x20
/* Kommandos */
#define STK_GET_SYNC       0x30
#define STK_GET_SIGN_ON    0x31
#define STK_SET_PARAMETER  0x40
#define STK_GET_PARAMETER  0x41
#define STK_SET_DEVICE     0x42
#define STK_SET_DEVICE_EXT 0x45
#define STK_ENTER_PROGMODE 0x50
#define STK_LEAVE_PROGMODE 0x51
#define STK_CHIP_ERASE     0x52
#define STK_LOAD_ADDRESS   0x55
#define STK_UNIVERSAL      0x56
#define STK_PROG_PAGE      0x64
#define STK_READ_PAGE      0x74
#define STK_READ_SIGN      0x75

static uint8_t SIGNATURE[3] = {0x1E, 0x95, 0x0F}; /* Standard: 328P; via Setter aenderbar */

void stk500_set_signature(uint8_t s0, uint8_t s1, uint8_t s2) {
    SIGNATURE[0] = s0;
    SIGNATURE[1] = s1;
    SIGNATURE[2] = s2;
}

static bool gb(Stk500Io* io, uint8_t* b) {
    while(*io->run) {
        if(io->get(io->ctx, b, 50)) return true;
    }
    return false;
}
static uint8_t g1(Stk500Io* io) {
    uint8_t b = 0;
    gb(io, &b);
    return b;
}
static void p1(Stk500Io* io, uint8_t b) {
    io->send(io->ctx, &b, 1);
}

/* EOP erwarten. true = synchron (dann Antwort senden), false -> NOSYNC bereits gesendet. */
static bool eop(Stk500Io* io) {
    uint8_t b;
    if(!gb(io, &b)) return false;
    if(b != CRC_EOP) {
        p1(io, STK_NOSYNC);
        return false;
    }
    return true;
}

void stk500_run(Stk500Io* io) {
    uint32_t byte_addr = 0;
    bool pmode = false;
    uint8_t buf[512];

    while(*io->run) {
        uint8_t cmd;
        if(!gb(io, &cmd)) break;

        switch(cmd) {
        case STK_GET_SYNC:
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;

        case STK_GET_SIGN_ON:
            if(eop(io)) {
                p1(io, STK_INSYNC);
                io->send(io->ctx, (const uint8_t*)"AVR ISP", 7);
                p1(io, STK_OK);
            }
            break;

        case STK_GET_PARAMETER: {
            uint8_t prm = g1(io);
            if(eop(io)) {
                uint8_t v = 0x00;
                if(prm == 0x80)
                    v = 0x02; /* HW-Version */
                else if(prm == 0x81)
                    v = 0x01; /* SW-Major */
                else if(prm == 0x82)
                    v = 0x12; /* SW-Minor */
                else if(prm == 0x84 || prm == 0x98)
                    v = 0x03;
                p1(io, STK_INSYNC);
                p1(io, v);
                p1(io, STK_OK);
            }
            break;
        }

        case STK_SET_PARAMETER:
            g1(io);
            g1(io);
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;

        case STK_SET_DEVICE:
            for(int i = 0; i < 20; i++)
                g1(io);
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;

        case STK_SET_DEVICE_EXT:
            for(int i = 0; i < 5; i++)
                g1(io);
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;

        case STK_ENTER_PROGMODE:
            if(eop(io)) {
                if(!pmode) pmode = lgt_pmode_enter(); /* Unlock + Erase (einmal) */
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
                if(io->activity) io->activity(io->ctx);
            }
            break;

        case STK_LEAVE_PROGMODE:
            if(eop(io)) {
                if(pmode) {
                    lgt_pmode_leave();
                    pmode = false;
                }
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;

        case STK_CHIP_ERASE: /* Unlock hat bereits geloescht */
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;

        case STK_LOAD_ADDRESS: {
            uint8_t lo = g1(io), hi = g1(io); /* Wortadresse, little-endian */
            byte_addr = (((uint32_t)hi << 8) | lo) * 2u; /* -> Byteadresse */
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;
        }

        case STK_UNIVERSAL: {
            uint8_t a = g1(io), b = g1(io), c = g1(io), d = g1(io);
            uint8_t r = 0x00;
            (void)b;
            (void)d;
            if(a == 0x30 && c < 3) r = SIGNATURE[c]; /* Read-Signature-Byte */
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, r);
                p1(io, STK_OK);
            }
            break;
        }

        case STK_PROG_PAGE: {
            uint16_t len = (uint16_t)g1(io) << 8;
            len |= g1(io);
            uint8_t mem = g1(io); /* 'F' Flash / 'E' EEPROM */
            if(len > sizeof(buf)) len = sizeof(buf);
            for(uint16_t i = 0; i < len; i++)
                buf[i] = g1(io);
            if(eop(io)) {
                if(mem == 'F' && pmode) lgt_page_write(byte_addr, buf, len);
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
                if(io->activity) io->activity(io->ctx);
            }
            break;
        }

        case STK_READ_PAGE: {
            uint16_t len = (uint16_t)g1(io) << 8;
            len |= g1(io);
            uint8_t mem = g1(io);
            if(len > sizeof(buf)) len = sizeof(buf);
            if(eop(io)) {
                p1(io, STK_INSYNC);
                if(mem == 'F' && pmode) {
                    lgt_page_read(byte_addr, buf, len);
                    io->send(io->ctx, buf, len);
                } else {
                    for(uint16_t i = 0; i < len; i++)
                        p1(io, 0xFF);
                }
                p1(io, STK_OK);
                if(io->activity) io->activity(io->ctx);
            }
            break;
        }

        case STK_READ_SIGN:
            if(eop(io)) {
                p1(io, STK_INSYNC);
                io->send(io->ctx, SIGNATURE, 3);
                p1(io, STK_OK);
            }
            break;

        default: /* unbekannt: nachsichtig quittieren */
            if(eop(io)) {
                p1(io, STK_INSYNC);
                p1(io, STK_OK);
            }
            break;
        }
    }
    if(pmode) lgt_pmode_leave();
}
