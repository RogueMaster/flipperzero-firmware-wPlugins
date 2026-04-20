// nmea.c — minimal RMC/GGA parser without atof/strtok_r (Flipper-safe)
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static float parse_float_simple(const char* s, size_t n) {
    // Parses a simple decimal with optional leading sign and dot (no exponent)
    if(!s || n == 0) return 0.0f;
    size_t i = 0;
    int sign = 1;
    if(s[i] == '+') { i++; }
    else if(s[i] == '-') { sign = -1; i++; }

    uint32_t ip = 0;
    while(i < n) {
        char c = s[i];
        if(c >= '0' && c <= '9') {
            ip = ip * 10u + (uint32_t)(c - '0');
            i++;
        } else {
            break;
        }
    }

    float val = (float)ip;
    if(i < n && s[i] == '.') {
        i++;
        float base = 0.1f;
        while(i < n) {
            char c = s[i];
            if(c >= '0' && c <= '9') {
                val += base * (float)(c - '0');
                base *= 0.1f;
                i++;
            } else {
                break;
            }
        }
    }
    return (float)sign * val;
}

static float nmea_degmin_to_deg_str(const char* s, size_t n) {
    // Convert numeric string DDMM.mmmm or DDDMM.mmmm to decimal degrees
    if(!s || n == 0) return 0.0f;
    // find dot to separate minutes precision
    size_t dot = 0;
    for(size_t i = 0; i < n; i++) {
        if(s[i] == '.') { dot = i; break; }
    }
    if(dot == 0) {
        // no dot found or dot at 0; fallback: parse as degrees directly
        return parse_float_simple(s, n);
    }
    // number of digits before dot (could include at least 4 or 5)
    size_t int_digits = dot;
    if(int_digits < 3) {
        return parse_float_simple(s, n);
    }
    // dd or ddd are degrees, the last two digits before dot belong to minutes
    // robust split: dd = floor(full/100), mm = full - dd*100
    float full = parse_float_simple(s, n);
    int dd = (int)(full / 100.0f);
    float mm = full - (float)(dd * 100);
    return (float)dd + (mm / 60.0f);
}

typedef struct {
    const char* p;
    size_t len;
} Slice;

static int split_csv_fields(const char* line, Slice* out, int max_fields) {
    int nf = 0;
    const char* s = line;
    const char* field_start = s;
    while(*s) {
        if(*s == ',' || *s == '\r' || *s == '\n') {
            if(nf < max_fields) {
                out[nf].p = field_start;
                out[nf].len = (size_t)(s - field_start);
                nf++;
            }
            if(*s == ',') {
                s++;
                field_start = s;
                continue;
            } else {
                // end of line
                break;
            }
        } else {
            s++;
        }
    }
    // last field if line doesn't end with comma or newline
    if(field_start && *field_start && nf < max_fields) {
        const char* end = s;
        if(end > field_start) {
            out[nf].p = field_start;
            out[nf].len = (size_t)(end - field_start);
            nf++;
        }
    }
    return nf;
}

static bool starts_with(const char* s, const char* prefix) {
    while(*prefix && *s) {
        if(*s++ != *prefix++) return false;
    }
    return *prefix == '\0';
}

void nmea_parse_line(
    const char* line,
    bool* has_fix,
    float* lat,
    float* lon,
    float* hdop,
    uint8_t* sats,
    float* altitude,
    float* heading_deg,
    float* speed_knots) {
    if(!line) return;

    if(has_fix) *has_fix = false;

    Slice f[20];
    int nf = split_csv_fields(line, f, 20);
    if(nf < 1) return;

    if(starts_with(line, "$GPRMC") || starts_with(line, "$GNRMC")) {
        // RMC: 0:$GPRMC,1:time,2:status(A/V),3:lat,4:N/S,5:lon,6:E/W,7:speed,8:course
        if(nf >= 7) {
            bool active = (f[2].len == 1 && (f[2].p[0] == 'A' || f[2].p[0] == 'a'));
            if(has_fix) *has_fix = active;
            if(active) {
                if(lat && f[3].len) {
                    float latv = nmea_degmin_to_deg_str(f[3].p, f[3].len);
                    if(f[4].len == 1 && (f[4].p[0] == 'S' || f[4].p[0] == 's')) latv = -latv;
                    *lat = latv;
                }
                if(lon && f[5].len) {
                    float lonv = nmea_degmin_to_deg_str(f[5].p, f[5].len);
                    if(f[6].len == 1 && (f[6].p[0] == 'W' || f[6].p[0] == 'w')) lonv = -lonv;
                    *lon = lonv;
                }
                if(speed_knots && nf >= 8 && f[7].len) {
                    *speed_knots = parse_float_simple(f[7].p, f[7].len);
                }
                if(heading_deg && nf >= 9 && f[8].len) {
                    *heading_deg = parse_float_simple(f[8].p, f[8].len);
                }
            }
        }
        return;
    }

    if(starts_with(line, "$GPGGA") || starts_with(line, "$GNGGA")) {
        // GGA: 0:$GPGGA,1:time,2:lat,3:N/S,4:lon,5:E/W,
        //      6:fix_quality,7:sats,8:hdop,9:altitude,10:unit(M)
        if(nf >= 9) {
            int quality = 0;
            if(f[6].len && f[6].p[0] >= '0' && f[6].p[0] <= '8') {
                quality = (int)(f[6].p[0] - '0');
            }
            bool fix_ok = (quality > 0) && f[2].len > 0 && f[4].len > 0;
            if(has_fix) *has_fix = fix_ok;

            if(fix_ok) {
                float latv = nmea_degmin_to_deg_str(f[2].p, f[2].len);
                if(f[3].len == 1 && (f[3].p[0] == 'S' || f[3].p[0] == 's')) latv = -latv;
                float lonv = nmea_degmin_to_deg_str(f[4].p, f[4].len);
                if(f[5].len == 1 && (f[5].p[0] == 'W' || f[5].p[0] == 'w')) lonv = -lonv;
                if(lat) *lat = latv;
                if(lon) *lon = lonv;
            }

            if(sats) {
                uint32_t v = 0;
                for(size_t i = 0; i < f[7].len; i++) {
                    char c = f[7].p[i];
                    if(c >= '0' && c <= '9') v = v * 10u + (uint32_t)(c - '0');
                    else break;
                }
                *sats = (uint8_t)(v & 0xFF);
            }
            if(hdop && f[8].len) {
                *hdop = parse_float_simple(f[8].p, f[8].len);
            }
            if(altitude && nf >= 10 && f[9].len) {
                *altitude = parse_float_simple(f[9].p, f[9].len);
            }
        }
        return;
    }
}
