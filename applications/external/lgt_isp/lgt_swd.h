/* ============================================================
 * lgt_swd.h  —  LGT8F328P SWD-Programmierprotokoll (GPIO-Bitbang)
 * Portiert 1:1 aus dem hardware-bestaetigten ft2232_lgtisp (nur die
 * unterste "ein SWD-Bit takten/lesen"-Ebene ist furi_hal_gpio statt MPSSE).
 * Das ist das proprietaere, reverse-engineerte LGT-Protokoll — NICHT ARM-SWD.
 * ============================================================ */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define LGT_FLASH_BYTES 32768u
#define LGT_PAGE_BYTES  128u

/* Fortschritts-Callback: done/total in Bytes, phase = LGT_PHASE_* (App uebersetzt). */
#define LGT_PHASE_WRITE  0
#define LGT_PHASE_VERIFY 1
#define LGT_PHASE_READ   2
typedef void (*LgtProgressCb)(void* ctx, uint32_t done, uint32_t total, int phase);

/* Halbbit-Verzoegerung in Mikrosekunden (Timing-Tuning; Default 2). Kleiner = schneller. */
void lgt_set_delay_us(uint32_t us);

/* GPIO einrichten / freigeben (SWC/SWD/RSTN als Ausgang, SWD spaeter bidirektional). */
void lgt_gpio_init(void);
void lgt_gpio_deinit(void);

/* --- High-Level (machen Reset/Unlock/Erase selbst) ---
 * Rueckgabe: >=0 = Zahl abweichender Bytes (0 = ok), -1 = kein LGT erkannt. */
int lgt_flash(const uint8_t* img, uint32_t img_len, bool verify, LgtProgressCb cb, void* ctx);
int lgt_verify(const uint8_t* img, uint32_t img_len, LgtProgressCb cb, void* ctx);

/* Nur die Chip-ID lesen (kein Erase). true = LGT erkannt, id[0]=0x3E/0x3F. */
bool lgt_read_id(uint8_t id[4]);

/* Wie lgt_read_id, zusaetzlich die lock-unabhaengige 4-Byte-GUID (Chip-Seriennummer).
 * guid ist mit 0 belegt, wenn nicht plausibel/nicht erkannt. Kein Erase. */
bool lgt_read_id_guid(uint8_t id[4], uint8_t guid[4]);

/* Leseschutz brechen (CRACK): loescht NUR die 1. Flash-Seite (1 KB, 0x000-0x3FF),
 * der Rest (0x400..) bleibt lesbar; danach ist der Chip in-Session entsperrt.
 * true = LGT erkannt, id = SWDID nach dem Crack. Semi-destruktiv! */
bool lgt_crack(uint8_t id[4]);

/* Flash auslesen ueber CRACK: opfert die 1. Seite (0x000-0x3FF -> liest 0xFF),
 * 0x400.. kommt im Klartext. buf muss len Bytes fassen. 0 = ok, -1 = kein LGT.
 * Ein frisch resetteter LGT ist immer gesperrt -> Auslesen erfordert diesen Crack. */
int lgt_dump(uint8_t* buf, uint32_t len, LgtProgressCb cb, void* ctx);

/* --- Low-Level fuer STK500/USB (persistente Programmiermodus-Session) ---
 * Zwischen enter/leave koennen beliebig viele page_write/page_read erfolgen. */
bool lgt_pmode_enter(void); /* GPIO-Init + Reset + Unlock(+Erase). true = im Progmode. */
void lgt_pmode_leave(void); /* Progmode verlassen + GPIO freigeben. */
void lgt_page_write(uint32_t byteAddr, const uint8_t* buf, int size);
void lgt_page_read(uint32_t byteAddr, uint8_t* buf, int size);
