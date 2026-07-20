/*
 * ================================================================
 *   si4703_detect.ino
 *   SI4703 FM-Tuner Detection & Debug — ESP32-WROOM
 *
 *   Protocol : Bit-banged I2C
 *              (SI4703 uses non-standard I2C — fixed address,
 *               read pointer starts at reg 0x0A and wraps.
 *               Hardware I2C controllers cannot handle this.)
 *
 *   Debug    : Serial Monitor  @ 115200 baud
 *
 *   After upload press any key in the Serial Monitor to
 *   re-run the full reset + detect + power-test cycle.
 * ================================================================
 *
 *   WIRING (matches Si4703 library pinout)
 *   ─────────────────────────────────────────────────────────
 *   SI4703 Pin      ESP32-WROOM GPIO     Notes
 *   ─────────────   ──────────────────   ────────────────────
 *   SDIO            GPIO 19              I2C Data + GPIO control
 *                                        (breakout has pull-ups)
 *   SCLK            GPIO 18              I2C Clock
 *                                        (breakout has pull-ups)
 *   RST             GPIO 21              **REQUIRED** Reset pin
 *                                        (active low, no pull-up)
 *   SEN             GPIO 22              Set HIGH for I2C mode
 *                                        (latched on RST edge)
 *   VCC             3.3 V                Do NOT use 5 V
 *   GND             GND
 *   ─────────────────────────────────────────────────────────
 * 
 *   NOTE: Breakout board includes pull-ups and crystal.
 *   ─────────────────────────────────────────────────────────
 *
 *   EXPECTED SERIAL OUTPUT (device present)
 *   ─────────────────────────────────────────────────────────
 *   DEVICEID : 0x1002   type=0x1  mfr=0x002
 *   CHIPID   : 0x1xxx   chip=0x1  ver & fw vary by unit
 *   Result   : [OK]  SI4703 detected!
 *   Power    : [OK]  Power enabled — device is fully responsive.
 *   ─────────────────────────────────────────────────────────
 *
 *   COMMON FAULT CODES
 *   ─────────────────────────────────────────────────────────
 *   All 0x0000  → No device responding  (wiring / power)
 *   All 0xFFFF  → Bus floating          (pull-ups missing)
 *   Random junk → Bad reset or glitch   (check RST / SEN timing)
 *   ─────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <Wire.h>

// ----------------------------------------------------------------
//  GPIO — ESP32 pin mapping for Si4703
//  Matches Si4703 library pinout:
//    SDIO -> ESP32 Pin 19 (I2C Data + GPIO control during powerup)
//    SCLK -> ESP32 Pin 18 (I2C Clock)
//    RST  -> ESP32 Pin 21 (Reset pin - active low, REQUIRED)
//  Note: SEN is pulled HIGH on SparkFun breakout board (no GPIO needed)
// ----------------------------------------------------------------
#define SDIO_PIN    19
#define SCLK_PIN    18
#define RST_PIN     21
#define SEN_PIN     22  // Optional - SparkFun board has SEN pulled high internally

// ----------------------------------------------------------------
//  SI4703 constants
// ----------------------------------------------------------------
// Wire uses 7-bit I2C addresses. SI4703 is 0x10 (7-bit).
#define SI4703_I2C_ADDR 0x10

/* Register indices 0x00 – 0x0F */
#define R_DEVICEID      0x00
#define R_CHIPID        0x01
#define R_POWERENABLE   0x02  // POWERCFG
#define R_CHANNEL       0x03
#define R_SYSCONFIG1    0x04
#define R_SYSCONFIG2    0x05
#define R_SYSCONFIG3    0x06
#define R_TEST1         0x07
#define R_TEST2         0x08
#define R_BOOTCONFIG    0x09
#define R_STATUSREGHI   0x0A  // STATUSRSSI
#define R_READCHAN      0x0B  // READCHAN (actual channel after tune/seek)
#define R_RDDS          0x0C  // RDSA
#define R_RDDS2         0x0D  // RDSB
#define R_RDDS3         0x0E  // RDSC
#define R_RDDS4         0x0F  // RDSD

/* Human-readable names for the dump table */
static const char* REG_NAME[] = {
    "DEVICEID",     "CHIPID",       "POWERCFG",     "CHANNEL",
    "SYSCONFIG1",   "SYSCONFIG2",   "SYSCONFIG3",   "TEST1",
    "TEST2",        "BOOTCONFIG",   "STATUSRSSI",   "READCHAN",
    "RDSA",         "RDSB",         "RDSC",         "RDSD"
};

/* Hardware read order: starts at 0x0A, wraps at 0x0F → 0x00 … 0x09 */
static const uint8_t RD_ORDER[] = {
    0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
};

/* Local shadow of all 16 registers */
static uint16_t reg[16];

// ================================================================
//  SI4703  —  reset / read / write
// ================================================================

/*
 * Reset sequence (per SparkFun/datasheet §4.1):
 *   1. SEN must be HIGH (SparkFun board pulls it high internally)
 *   2. SDIO pulled LOW → boot-strap condition for 2-wire I2C
 *   3. RST toggled LOW → HIGH
 *   4. Release SDIO/SCLK to INPUT_PULLUP
 *   5. Wait ≥ 110 ms for internal power-on calibration
 */
static void si4703_reset(void) {
    Serial.println("[RST] Starting SparkFun-style reset sequence …");
    
    // Set SEN high if using external control (SparkFun board has pullup)
    pinMode(SEN_PIN, OUTPUT);  
    digitalWrite(SEN_PIN, HIGH);
    delay(1);
    
    // Configure RST and assert it (pull low)
    pinMode(RST_PIN, OUTPUT);
    digitalWrite(RST_PIN, LOW);
    delay(1);
    
    // Hold SDIO low during reset (selects 2-wire I2C interface)
    pinMode(SDIO_PIN, OUTPUT);
    digitalWrite(SDIO_PIN, LOW);
    delay(1);

    // Per datasheet guidance, keep SCLK high when releasing reset
    pinMode(SCLK_PIN, OUTPUT);
    digitalWrite(SCLK_PIN, HIGH);
    delay(1);
    
    // Release reset while SDIO is low
    Serial.println("[RST] Releasing reset with SDIO held low …");
    digitalWrite(RST_PIN, HIGH);

    // Quickly release SDIO/SCLK so pull-ups can take over before I2C starts
    pinMode(SDIO_PIN, INPUT_PULLUP);
    pinMode(SCLK_PIN, INPUT_PULLUP);
    delay(2);
    
    // Now release SDIO and SCLK back to I2C control
    Serial.println("[RST] Initializing Wire (I2C) …");
    Wire.begin(SDIO_PIN, SCLK_PIN);  // Initialize I2C with custom pins
    Wire.setClock(100000);            // 100kHz I2C clock (standard mode)
    
    // Wait for chip to complete internal initialization
    delay(110);  // Datasheet specifies 110ms max for powerup

    Serial.println("[RST] Reset complete. I2C ready for communication.");
}

/*
 * Read all 16 registers (32 bytes) using Wire library.
 * The SI4703 read pointer always starts at 0x0A; we re-map
 * each pair of bytes back into reg[0..15] via RD_ORDER[].
 */
static bool si4703_read(void) {
    Serial.println("[I2C] Requesting 32 bytes from Si4703 …");
    
    // Request 32 bytes from SI4703 (starts reading from 0x0A)
    Wire.requestFrom(SI4703_I2C_ADDR, 32);
    
    if (Wire.available() < 32) {
        Serial.printf("[I2C] ERROR: Only %d bytes available (expected 32)\n", Wire.available());
        return false;
    }
    
    // Read all 32 bytes and rearrange according to RD_ORDER
    for (int i = 0; i < 16; i++) {
        uint8_t hi = Wire.read();
        uint8_t lo = Wire.read();
        reg[RD_ORDER[i]] = ((uint16_t)hi << 8) | lo;
    }
    
    Serial.println("[I2C] Read complete.");
    return true;
}

/*
 * Write registers 0x02 – 0x07 (the writable range) using Wire.
 * Write pointer always starts at 0x02; we send 12 bytes (6 regs) in order.
 */
static void si4703_write(void) {
    Serial.println("[I2C] Writing regs 0x02 – 0x07 …");
    // Debug: Show key registers before write
    Serial.printf("[I2C]   TEST1 (0x07) = 0x%04X\n", reg[R_TEST1]);
    Serial.printf("[I2C]   POWERCFG (0x02) = 0x%04X\n", reg[R_POWERENABLE]);
    Serial.printf("[I2C]   CHANNEL (0x03) = 0x%04X\n", reg[R_CHANNEL]);

    Wire.beginTransmission(SI4703_I2C_ADDR);
    
    // Write 6 registers (12 bytes) starting from 0x02
    for (int r = 0x02; r <= 0x07; r++) {
        Wire.write((reg[r] >> 8) & 0xFF);   // MSB
        Wire.write( reg[r]       & 0xFF);   // LSB
    }
    
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
        Serial.println("[I2C] Write complete.");
    } else {
        Serial.printf("[I2C] ERROR: Write failed with code %d\n", error);
    }
}

// ================================================================
//  Debug helpers
// ================================================================

/* Print all 16 registers in an ASCII table */
static void dump_regs(void) {
    Serial.println();
    Serial.println("  +--------+------------------+----------+");
    Serial.println("  |  Reg#  |       Name       |  Value   |");
    Serial.println("  +--------+------------------+----------+");
    for (int i = 0; i < 16; i++) {
        Serial.printf("  |  0x%02X  |  %-16s|  0x%04X  |\n",
                      i, REG_NAME[i], reg[i]);
    }
    Serial.println("  +--------+------------------+----------+");
}

/* Pretty-print the identification fields from DEVICEID & CHIPID */
static void print_id(void) {
    uint16_t did  = reg[R_DEVICEID];
    uint16_t cid  = reg[R_CHIPID];

    uint8_t devType  = (did >> 12) & 0x0F;
    uint16_t mfr     =  did        & 0x0FFF;
    uint8_t chipType = (cid >> 12) & 0x0F;
    uint8_t chipVer  = (cid >>  6) & 0x3F;
    uint8_t fwVer    =  cid        & 0x3F;

    Serial.println("\n[ID] ── Breakdown ──");
    Serial.printf("  DEVICEID  0x%04X   type = 0x%X   mfr  = 0x%03X\n",
                  did, devType, mfr);
    Serial.printf("  CHIPID    0x%04X   chip = 0x%X   ver  = 0x%02X   fw = 0x%02X\n",
                  cid, chipType, chipVer, fwVer);

    /* Expected for SI4703: type=1, mfr=0x002, chip=1 */
    Serial.println("\n[ID] ── Result ──");
    if (did == 0x0000 && cid == 0x0000) {
        Serial.println("  [FAIL] All zeros — no device responded.");
        Serial.println("         Check power (3.3 V), SDIO / SCLK wiring,");
        Serial.println("         and pull-up resistors.");

    } else if (did == 0xFFFF && cid == 0xFFFF) {
        Serial.println("  [FAIL] All ones — bus is floating.");
        Serial.println("         Add 4.7 kΩ pull-ups on SDIO and SCLK.");

    } else if (devType == 0x1 && mfr == 0x002 && chipType == 0x1) {
        Serial.printf("  [OK]   SI4703 detected!  "
                      "Chip ver 0x%02X / FW 0x%02X\n", chipVer, fwVer);

    } else {
        Serial.println("  [WARN] Unrecognised ID — a device may be present");
        Serial.println("         but does not match SI4703 signature.");
        Serial.println("         Re-check reset wiring (SEN LOW, SDIO LOW");
        Serial.println("         before RST rises) and pull-ups.");
    }
}

// ================================================================
//  FM Radio Control Functions
// ================================================================

/*
 * Set frequency in MHz (e.g., 95.7 for 95.7 FM)
 * Band spacing: 100kHz (US) or 200kHz (depends on SYSCONFIG2)
 */
static void set_frequency(float freq_mhz) {
    si4703_read();
    
    // Calculate channel number from frequency
    // Channel = (Freq - 87.5) / 0.1  for 100kHz spacing
    int channel = (int)((freq_mhz - 87.5) / 0.1);
    
    Serial.printf("[TUNE] Setting frequency to %.1f MHz (channel %d) \u2026\n", freq_mhz, channel);
    Serial.printf("[TUNE] POWERENABLE = 0x%04X (ENABLE=%d)\n", 
                  reg[R_POWERENABLE], (reg[R_POWERENABLE] & 0x0001) ? 1 : 0);
    Serial.printf("[TUNE] TEST1 = 0x%04X (XOSCEN=%d)\n", 
                  reg[R_TEST1], (reg[R_TEST1] & 0x8000) ? 1 : 0);
    
    // Set the channel in CHANNEL register and enable TUNE bit
    reg[R_CHANNEL] = (1u << 15) | (channel & 0x03FF);  // TUNE bit + channel
    Serial.printf("[TUNE] Writing CHANNEL = 0x%04X\n", reg[R_CHANNEL]);
    si4703_write();
    
    // Poll for STC (Seek/Tune Complete) bit
    Serial.print("[TUNE] Waiting for STC ");
    int timeout = 0;
    bool tune_complete = false;
    while (timeout < 20) {  // 2 second timeout
        delay(100);
        si4703_read();
        Serial.print(".");
        if (reg[R_STATUSREGHI] & 0x4000) {  // STC bit
            tune_complete = true;
            break;
        }
        timeout++;
    }
    Serial.println();
    
    Serial.printf("[TUNE] STATUSREGHI = 0x%04X (STC=%d, SFBL=%d)\n", 
                  reg[R_STATUSREGHI],
                  (reg[R_STATUSREGHI] & 0x4000) ? 1 : 0,
                  (reg[R_STATUSREGHI] & 0x2000) ? 1 : 0);
    
    if (tune_complete) {
        Serial.println("[TUNE] Tune complete!");
    } else {
        Serial.println("[TUNE] ERROR: Tune timeout - STC bit not set");
        Serial.println("[TUNE] Possible causes:");
        Serial.println("       - Oscillator not running (check TEST1)");
        Serial.println("       - ENABLE bit not set (check POWERENABLE)");
        Serial.println("       - Hardware issue");
    }
    
    // Clear TUNE bit to continue operation
    si4703_read();
    reg[R_CHANNEL] &= ~(1u << 15);
    si4703_write();
    delay(10);
}

/*
 * Set volume (0-15)
 */
static void set_volume(uint8_t vol) {
    if (vol > 15) vol = 15;
    
    si4703_read();
    reg[R_SYSCONFIG2] = (reg[R_SYSCONFIG2] & 0xFFF0) | vol;
    si4703_write();
    
    Serial.printf("[VOL] Volume set to %d\n", vol);
}

/*
 * Seek up or down
 */
static void seek(bool seek_up) {
    si4703_read();
    
    Serial.printf("[SEEK] Seeking %s \u2026\n", seek_up ? "UP" : "DOWN");
    
    // Set seek direction and enable seek
    if (seek_up) {
        reg[R_POWERENABLE] |= (1u << 9);   // SEEKUP = 1
    } else {
        reg[R_POWERENABLE] &= ~(1u << 9);  // SEEKUP = 0
    }
    reg[R_POWERENABLE] |= (1u << 8);      // SEEK = 1
    si4703_write();
    
    // Poll for STC (Seek/Tune Complete)
    int timeout = 0;
    bool seek_complete = false;
    while (timeout < 80) {  // 8 second timeout for seek
        delay(100);
        si4703_read();
        if (reg[R_STATUSREGHI] & 0x4000) {  // STC bit set
            seek_complete = true;
            break;
        }
        timeout++;
    }
    
    if (!seek_complete) {
        Serial.println("[SEEK] Seek timeout");
    }
    
    // Check seek results
    si4703_read();
    if (reg[R_STATUSREGHI] & 0x2000) {  // SFBL bit
        Serial.println("[SEEK] Seek failed - band limit reached");
    } else if (seek_complete) {
        // Get the new frequency
        int channel = reg[R_READCHAN] & 0x03FF;
        float freq = 87.5 + (channel * 0.1);
        uint8_t rssi = reg[R_STATUSREGHI] & 0x00FF;
        Serial.printf("[SEEK] Found station at %.1f MHz (RSSI: %d)\n", freq, rssi);
    }
    
    // Clear SEEK bit
    si4703_read();
    reg[R_POWERENABLE] &= ~(1u << 8);
    si4703_write();
    delay(10);
}

/*
 * Get current frequency
 */
static float get_frequency(void) {
    si4703_read();
    int channel = reg[R_READCHAN] & 0x03FF;
    return 87.5 + (channel * 0.1);
}

/*
 * Get RSSI (signal strength)
 */
static uint8_t get_rssi(void) {
    si4703_read();
    return reg[R_STATUSREGHI] & 0x00FF;
}

/*
 * Display current station info
 */
static void display_info(void) {
    si4703_read();
    
    float freq = get_frequency();
    uint8_t rssi = get_rssi();
    bool stereo = (reg[R_STATUSREGHI] & 0x0100) != 0;
    uint8_t volume = reg[R_SYSCONFIG2] & 0x000F;
    
    Serial.println();
    Serial.println("\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500");
    Serial.printf("  Frequency : %.1f MHz\n", freq);
    Serial.printf("  RSSI      : %d\n", rssi);
    Serial.printf("  Mode      : %s\n", stereo ? "STEREO" : "MONO");
    Serial.printf("  Volume    : %d/15\n", volume);
    Serial.println("\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500");
    Serial.println();
}

// ================================================================
//  Detection + power-enable test
// ================================================================

static void run_detect(void) {
    Serial.println("\n--- Detection -----------------------------------------");
    si4703_read();
    dump_regs();
    print_id();
}

/*
 * Enable oscillator and power up the chip with audio unmuted.
 */
static void run_power_test(void) {
    Serial.println("\n--- Power-Enable Test ---------------------------------");

    si4703_read();
    Serial.printf("[PWR] POWERENABLE before : 0x%04X\n", reg[R_POWERENABLE]);

    // Enable oscillator first (critical for Si4703 operation)
    Serial.println("[PWR] Enabling crystal oscillator (0x8100) …");
    reg[R_TEST1] = 0x8100;  // Enable oscillator
    si4703_write();
    delay(500);  // Wait 500ms for oscillator to stabilize

    si4703_read();
    Serial.printf("[PWR] TEST1 register    : 0x%04X\n", reg[R_TEST1]);

    // Now enable the chip and unmute
    reg[R_POWERENABLE] = 0x4001;  // DMUTE (unmute) + ENABLE bits
    si4703_write();
    delay(110);  // Wait for powerup

    si4703_read();
    Serial.printf("[PWR] POWERENABLE after  : 0x%04X\n", reg[R_POWERENABLE]);

    dump_regs();

    if (reg[R_POWERENABLE] & 0x0001) {
        Serial.println("\n[PWR] [OK]   ENABLE bit is set — device is fully responsive.");
        if (reg[R_POWERENABLE] & 0x4000) {
            Serial.println("[PWR] [OK]   Audio is UNMUTED.");
        }
    } else {
        Serial.println("\n[PWR] [WARN] ENABLE bit cleared — check VCC and wiring.");
    }
}

// ================================================================
//  Sketch entry-points
// ================================================================

void setup(void) {
    Serial.begin(115200);
    // Some ESP32 builds don't block on Serial; short wait is fine.
    delay(100);

    Serial.println("\n================================================");
    Serial.println("  SI4703 Detection  —  ESP32-WROOM");
    Serial.println("  Serial Monitor  :  115200 baud");
    Serial.println("  Pin Configuration (matches Si4703 library):");
    Serial.println("    SDIO -> GPIO 19   SCLK -> GPIO 18");
    Serial.println("    RST  -> GPIO 21   SEN  -> GPIO 22 (HIGH)");
    Serial.println("================================================");
    Serial.printf("\n[INIT] GPIO  SDIO=%d   SCLK=%d   RST=%d   SEN=%d\n",
                  SDIO_PIN, SCLK_PIN, RST_PIN, SEN_PIN);

    si4703_reset();
    run_detect();
    run_power_test();  // This now enables oscillator and unmutes
    
    // Configure FM band and spacing
    Serial.println("\n[INIT] Configuring FM radio settings \u2026");
    si4703_read();
    // SYSCONFIG2: Seek threshold=0x19, US band (87.5-108), 100kHz spacing, volume=7
    reg[R_SYSCONFIG2] = (0x19 << 8) | (0x00 << 6) | (0x01 << 4) | 0x07;
    // SYSCONFIG1: RDS enabled, 50us de-emphasis
    reg[R_SYSCONFIG1] = 0x1000;
    si4703_write();
    delay(10);
    
    // Set initial frequency to 95.5 MHz
    set_frequency(95.5);
    display_info();

    Serial.println("\n[INFO] FM Radio Commands:");
    Serial.println("  i - Show current station info");
    Serial.println("  u - Seek UP to next station");
    Serial.println("  d - Seek DOWN to previous station");
    Serial.println("  + - Increase volume");
    Serial.println("  - - Decrease volume");
    Serial.println("  t - Tune to specific frequency (e.g., t 95.7)");
    Serial.println("  r - Re-run detection test\n");
}

void loop(void) {
    if (Serial.available()) {
        char cmd = Serial.read();
        
        // Flush remaining input
        while (Serial.available() && Serial.peek() == ' ') {
            Serial.read();
        }
        
        switch (cmd) {
            case 'i':  // Info
                display_info();
                break;
                
            case 'u':  // Seek up
                seek(true);
                display_info();
                break;
                
            case 'd':  // Seek down
                seek(false);
                display_info();
                break;
                
            case '+':  // Volume up
                {
                    si4703_read();
                    uint8_t vol = (reg[R_SYSCONFIG2] & 0x000F);
                    if (vol < 15) set_volume(vol + 1);
                }
                break;
                
            case '-':  // Volume down
                {
                    si4703_read();
                    uint8_t vol = (reg[R_SYSCONFIG2] & 0x000F);
                    if (vol > 0) set_volume(vol - 1);
                }
                break;
                
            case 't':  // Tune to frequency
                {
                    // Read frequency from serial
                    float freq = Serial.parseFloat();
                    if (freq >= 87.5 && freq <= 108.0) {
                        set_frequency(freq);
                        display_info();
                    } else {
                        Serial.println("[ERROR] Frequency must be between 87.5 and 108.0 MHz");
                        Serial.println("        Example: t 95.7");
                    }
                }
                break;
                
            case 'r':  // Re-run detection
                Serial.println("\n=============== Re-scan triggered ===============");
                si4703_reset();
                run_detect();
                run_power_test();
                set_frequency(95.5);
                display_info();
                break;
                
            case '\n':
            case '\r':
                // Ignore newlines
                break;
                
            default:
                Serial.println("[CMD] Unknown command. Available commands:");
                Serial.println("  i, u, d, +, -, t <freq>, r");
                break;
        }
        
        // Flush any remaining input
        while (Serial.read() != -1) {}
    }
    delay(50);
}
