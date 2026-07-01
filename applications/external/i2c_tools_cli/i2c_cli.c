#include "i2c_cli.h"

#include <furi.h>
#include <furi_hal.h>
#include <cli/cli.h>
#include <toolbox/cli/cli_registry.h>
#include <toolbox/args.h>
#include <stdlib.h>
#include <string.h>

#define I2C_CLI_BUS     &furi_hal_i2c_handle_external
#define I2C_CLI_TIMEOUT 50
#define CLI_READ_MAX    256
#define CLI_WRITE_MAX   32

static void i2c_cli_print_usage(void) {
    printf("Usage:\r\n");
    printf("  i2c scan                                  - scan all 7-bit addresses\r\n");
    printf("  i2c probe <addr>                          - check if device responds\r\n");
    printf("  i2c read  <addr> <reg> <count> [hex|ascii] - read <count> bytes from <reg>\r\n");
    printf("  i2c write <addr> <reg> <byte> [<byte>...] - write bytes starting at <reg>\r\n");
    printf("All numbers are hex. Accepted forms: 1A, 1a, 0x1A, 0X1a, 0x1a.\r\n");
    printf("Wiring: SCL=C0 SDA=C1 GND=GND, 3V3 levels only.\r\n");
}

// Parse a hex unsigned integer from the next whitespace-delimited token in args.
// Accepts an optional 0x or 0X prefix and any mix of upper/lower hex digits.
// Returns false if no token, empty token, or invalid characters.
static bool parse_uint(FuriString* args, uint32_t* out) {
    FuriString* word = furi_string_alloc();
    bool ok = args_read_string_and_trim(args, word);
    if(ok) {
        const char* s = furi_string_get_cstr(word);
        if(s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
        if(*s == '\0') {
            ok = false;
        } else {
            char* end = NULL;
            unsigned long v = strtoul(s, &end, 16);
            ok = (end != NULL) && (*end == '\0');
            if(ok) *out = (uint32_t)v;
        }
    }
    furi_string_free(word);
    return ok;
}

static bool parse_byte(FuriString* args, uint8_t* out) {
    uint32_t v = 0;
    if(!parse_uint(args, &v)) return false;
    if(v > 0xFF) return false;
    *out = (uint8_t)v;
    return true;
}

static void cmd_scan(void) {
    printf("Scanning I2C bus...\r\n");
    uint8_t found = 0;
    furi_hal_i2c_acquire(I2C_CLI_BUS);
    for(uint8_t addr = 1; addr <= 0x7F; addr++) {
        if(furi_hal_i2c_is_device_ready(I2C_CLI_BUS, addr << 1, I2C_CLI_TIMEOUT)) {
            printf("  0x%02X\r\n", addr);
            found++;
        }
    }
    furi_hal_i2c_release(I2C_CLI_BUS);
    printf("Done. %u device(s) found.\r\n", (unsigned)found);
}

static void cmd_probe(FuriString* args) {
    uint8_t addr = 0;
    if(!parse_byte(args, &addr) || addr > 0x7F) {
        printf("usage: i2c probe <addr>\r\n");
        return;
    }
    furi_hal_i2c_acquire(I2C_CLI_BUS);
    bool ready = furi_hal_i2c_is_device_ready(I2C_CLI_BUS, addr << 1, I2C_CLI_TIMEOUT);
    furi_hal_i2c_release(I2C_CLI_BUS);
    printf("0x%02X: %s\r\n", addr, ready ? "present" : "absent");
}

static void cmd_read(FuriString* args) {
    uint8_t addr = 0;
    uint8_t reg = 0;
    uint32_t count = 0;
    if(!parse_byte(args, &addr) || addr > 0x7F) goto bad;
    if(!parse_byte(args, &reg)) goto bad;
    if(!parse_uint(args, &count)) goto bad;
    if(count == 0 || count > CLI_READ_MAX) {
        printf("count must be in 1..%u\r\n", (unsigned)CLI_READ_MAX);
        return;
    }

    // Optional format: "hex" (default) or "ascii"
    bool ascii = false;
    FuriString* fmt = furi_string_alloc();
    if(args_read_string_and_trim(args, fmt)) {
        const char* f = furi_string_get_cstr(fmt);
        if(strcmp(f, "ascii") == 0 || strcmp(f, "ASCII") == 0) {
            ascii = true;
        } else if(strcmp(f, "hex") != 0 && strcmp(f, "HEX") != 0) {
            printf("unknown format '%s' (expected 'hex' or 'ascii')\r\n", f);
            furi_string_free(fmt);
            return;
        }
    }
    furi_string_free(fmt);

    uint8_t buf[CLI_READ_MAX];
    furi_hal_i2c_acquire(I2C_CLI_BUS);
    bool ok = furi_hal_i2c_trx(I2C_CLI_BUS, addr << 1, &reg, 1, buf, count, I2C_CLI_TIMEOUT);
    furi_hal_i2c_release(I2C_CLI_BUS);
    if(!ok) {
        printf("read failed (no ACK?)\r\n");
        return;
    }
    if(ascii) {
        for(uint32_t i = 0; i < count; i++) {
            uint8_t b = buf[i];
            char c = (b >= 0x20 && b <= 0x7E) ? (char)b : '.';
            putchar(c);
        }
        printf("\r\n");
    } else {
        for(uint32_t i = 0; i < count; i++) {
            printf("%02X", buf[i]);
            bool last = (i + 1 == count);
            bool wrap = ((i + 1) % 16 == 0);
            printf("%s", (last || wrap) ? "\r\n" : " ");
        }
    }
    return;
bad:
    printf("usage: i2c read <addr> <reg> <count> [hex|ascii]\r\n");
}

static void cmd_write(FuriString* args) {
    uint8_t addr = 0;
    uint8_t reg = 0;
    if(!parse_byte(args, &addr) || addr > 0x7F) goto bad;
    if(!parse_byte(args, &reg)) goto bad;
    uint8_t buf[1 + CLI_WRITE_MAX];
    buf[0] = reg;
    uint8_t n = 1;
    while(n < (1 + CLI_WRITE_MAX)) {
        if(!parse_byte(args, &buf[n])) break;
        n++;
    }
    if(n == 1) goto bad;
    furi_hal_i2c_acquire(I2C_CLI_BUS);
    bool ok = furi_hal_i2c_tx(I2C_CLI_BUS, addr << 1, buf, n, I2C_CLI_TIMEOUT);
    furi_hal_i2c_release(I2C_CLI_BUS);
    printf(
        "write %u byte(s) to 0x%02X[0x%02X]: %s\r\n",
        (unsigned)(n - 1),
        addr,
        reg,
        ok ? "ok" : "failed");
    return;
bad:
    printf("usage: i2c write <addr> <reg> <byte> [<byte>...]\r\n");
}

static void i2c_cli_callback(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(pipe);
    UNUSED(context);
    FuriString* sub = furi_string_alloc();
    if(!args_read_string_and_trim(args, sub)) {
        i2c_cli_print_usage();
        furi_string_free(sub);
        return;
    }
    const char* s = furi_string_get_cstr(sub);
    if(strcmp(s, "scan") == 0) {
        cmd_scan();
    } else if(strcmp(s, "probe") == 0) {
        cmd_probe(args);
    } else if(strcmp(s, "read") == 0) {
        cmd_read(args);
    } else if(strcmp(s, "write") == 0) {
        cmd_write(args);
    } else if(strcmp(s, "help") == 0 || strcmp(s, "?") == 0) {
        i2c_cli_print_usage();
    } else {
        printf("Unknown subcommand '%s'.\r\n", s);
        i2c_cli_print_usage();
    }
    furi_string_free(sub);
}

void i2c_cli_register(void) {
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(
        registry, "i2c", CliCommandFlagParallelSafe, i2c_cli_callback, NULL);
    furi_record_close(RECORD_CLI);
}

void i2c_cli_unregister(void) {
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_delete_command(registry, "i2c");
    furi_record_close(RECORD_CLI);
}
