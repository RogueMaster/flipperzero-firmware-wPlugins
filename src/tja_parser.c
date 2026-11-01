// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#include "tja_parser.h"
#include "taiko_memory.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NOTES    8192
#define MAX_SLOTS    2048
#define MAX_COMMANDS 64
#define MAX_TIME_MS  3600000.0f

static void* chart_alloc(size_t size) {
    return taiko_alloc(size, 16 * 1024);
}

static void* chart_resize(void* data, size_t used, size_t size) {
    void* replacement = chart_alloc(size);
    if(!replacement) return NULL;
    if(used) memcpy(replacement, data, used);
    free(data);
    return replacement;
}

typedef struct {
    size_t slot;
    float value;
    enum {
        CommandBpm,
        CommandMeasure,
        CommandScroll,
        CommandDelay,
        CommandGogo,
        CommandBarline
    } kind;
} MeasureCommand;

typedef struct {
    char slots[MAX_SLOTS];
    size_t slot_count;
    MeasureCommand commands[MAX_COMMANDS];
    size_t command_count;
    size_t note_capacity;
    size_t bpm_capacity;
    size_t gogo_capacity;
    size_t roll_capacity;
    size_t bar_capacity;
    bool barline;
    size_t active_roll; /* index + 1, zero when inactive */
    char balloons[2048];
    char* next_balloon;
    float bpm;
    float scroll;
    float beats;
    float time_ms;
} Parser;

static char* trim(char* s) {
    while(isspace((unsigned char)*s))
        s++;
    size_t len = strlen(s);
    while(len && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
    return s;
}

const char* tja_course_name(TjaCourse course) {
    static const char* names[] = {"Easy", "Normal", "Hard", "Oni", "Edit", "Tower", "Dan"};
    return course < TjaCourseCount ? names[course] : "?";
}

static bool same_name(const char* a, const char* b) {
    while(*a && *b) {
        if(tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return false;
    }
    return *a == *b;
}

static bool read_course(const char* text, TjaCourse* course) {
    for(int i = 0; i < TjaCourseCount; i++) {
        if(same_name(text, tja_course_name(i)) || (text[0] == '0' + i && !text[1])) {
            *course = i;
            return true;
        }
    }
    if(same_name(text, "Ura")) {
        *course = TjaCourseEdit;
        return true;
    }
    return false;
}

TjaCourse tja_default_course(uint8_t courses) {
    if(courses & (1U << TjaCourseOni)) return TjaCourseOni;
    for(int i = TjaCourseCount - 1; i >= 0; i--)
        if(courses & (1U << i)) return i;
    return TjaCourseOni;
}

static char* clean_line(char* buffer, bool* first_line) {
    char* line = buffer;
    if(*first_line && strncmp(line, "\xef\xbb\xbf", 3) == 0) line += 3;
    *first_line = false;
    char* comment = strstr(line, "//");
    if(comment) *comment = '\0';
    return trim(line);
}

static bool is_start(const char* line) {
    return strcmp(line, "#START") == 0 || strncmp(line, "#START ", 7) == 0;
}

bool tja_list_courses(TjaReadLine reader, void* context, uint8_t* courses, const char** error) {
    *courses = 0;
    *error = "Missing #START or #END";
    char* buffer = chart_alloc(2048);
    if(!buffer) {
        *error = "Not enough memory";
        return false;
    }
    TjaCourse course = TjaCourseOni;
    bool inside = false, first_line = true, valid = true;
    int result;
    while((result = reader(context, buffer, 2048)) > 0) {
        char* line = clean_line(buffer, &first_line);
        if(inside) {
            if(strcmp(line, "#END") == 0) inside = false;
            continue;
        }
        if(is_start(line)) {
            *courses |= 1U << course;
            inside = true;
        } else {
            char* colon = strchr(line, ':');
            if(!colon) continue;
            *colon++ = '\0';
            if(strcmp(trim(line), "COURSE") == 0 && !read_course(trim(colon), &course)) {
                *error = "Unsupported COURSE";
                valid = false;
                break;
            }
        }
    }
    if(result < 0) *error = "TJA read error / line too long";
    bool success = valid && result == 0 && !inside && *courses;
    if(success) *error = NULL;
    free(buffer);
    return success;
}

static bool number(const char* text, float* value) {
    char* end;
    *value = strtof(text, &end);
    while(isspace((unsigned char)*end))
        end++;
    return end != text && !*end && isfinite(*value);
}

static bool add_note(Parser* p, TjaChart* chart, char kind, const char** error) {
    if(kind == '0') return true;
    if(kind == '8') {
        if(p->active_roll) {
            chart->rolls[p->active_roll - 1].end_ms = (int32_t)lroundf(p->time_ms);
            p->active_roll = 0;
        }
        return true;
    }
    if(p->active_roll) {
        TjaRoll* active = &chart->rolls[p->active_roll - 1];
        /* A repeated kusudama 9 changes scoring, rather than starting a note. */
        if(kind == '9' && active->kind == 9) return true;
        /* The next playable note takes priority, even if 8 is missing or late.
         * The exclusive end also prevents an extra automatic hit at this time. */
        active->end_ms = (int32_t)lroundf(p->time_ms);
        p->active_roll = 0;
    }
    float speed = p->bpm / TAIKO_SCROLL_REFERENCE_BPM * p->scroll;
    if(!isfinite(speed) || speed == 0) {
        *error = "Scroll speed out of range";
        return false;
    }
    if(chart->note_count + chart->roll_count == MAX_NOTES) {
        *error = "Chart exceeds 8192 notes";
        return false;
    }
    if(kind >= '5') {
        unsigned long balloon = 0;
        if(kind == '7' || kind == '9') {
            char* start = p->next_balloon ? trim(p->next_balloon) : p->balloons;
            char* end;
            if(!*start) {
                /* Match incomplete legacy charts without rejecting the whole song. */
                balloon = 5;
            } else {
                balloon = strtoul(start, &end, 10);
                while(isspace((unsigned char)*end))
                    end++;
                if(!isdigit((unsigned char)*start) || end == start || !balloon ||
                   balloon > UINT16_MAX || (*end && *end != ',')) {
                    *error = "Missing / invalid BALLOON count";
                    return false;
                }
                p->next_balloon = *end ? end + 1 : end;
            }
        }
        if(chart->roll_count == p->roll_capacity) {
            size_t capacity = p->roll_capacity ? p->roll_capacity * 2 : 16;
            TjaRoll* rolls = chart_resize(
                chart->rolls, chart->roll_count * sizeof(TjaRoll), capacity * sizeof(TjaRoll));
            if(!rolls) {
                *error = "Not enough memory";
                return false;
            }
            chart->rolls = rolls;
            p->roll_capacity = capacity;
        }
        chart->rolls[chart->roll_count++] = (TjaRoll){
            .time_ms = (int32_t)lroundf(p->time_ms),
            .scroll_speed = speed,
            .balloon_count = (uint16_t)balloon,
            .kind = (uint8_t)(kind - '0')};
        p->active_roll = chart->roll_count;
        return true;
    }
    if(chart->note_count == p->note_capacity) {
        size_t capacity = p->note_capacity ? p->note_capacity * 2 : 256;
        TjaNote* notes = chart_resize(
            chart->notes, chart->note_count * sizeof(TjaNote), capacity * sizeof(TjaNote));
        if(!notes) {
            *error = "Not enough memory";
            return false;
        }
        chart->notes = notes;
        p->note_capacity = capacity;
    }
    chart->notes[chart->note_count++] = (TjaNote){
        .time_ms = (int32_t)lroundf(p->time_ms),
        .kind = (uint8_t)(kind - '0'),
        .scroll_speed = speed};
    if(kind >= '1' && kind <= '4' && fabsf(speed) < chart->min_note_speed)
        chart->min_note_speed = fabsf(speed);
    return true;
}

static bool
    apply_measure_command(Parser* p, TjaChart* chart, const MeasureCommand* c, const char** error) {
    if(c->kind == CommandBpm) {
        if(chart->bpm_change_count == p->bpm_capacity) {
            size_t capacity = p->bpm_capacity ? p->bpm_capacity * 2 : 16;
            TjaBpmChange* changes = chart_resize(
                chart->bpm_changes,
                chart->bpm_change_count * sizeof(TjaBpmChange),
                capacity * sizeof(TjaBpmChange));
            if(!changes) {
                *error = "Not enough memory";
                return false;
            }
            chart->bpm_changes = changes;
            p->bpm_capacity = capacity;
        }
        chart->bpm_changes[chart->bpm_change_count++] =
            (TjaBpmChange){.time_ms = (int32_t)lroundf(p->time_ms), .bpm = c->value};
        p->bpm = c->value;
    } else if(c->kind == CommandGogo) {
        bool active = c->value != 0;
        bool previous = chart->gogo_change_count &&
                        chart->gogo_changes[chart->gogo_change_count - 1].active;
        if(active != previous) {
            if(chart->gogo_change_count == p->gogo_capacity) {
                if(p->gogo_capacity == MAX_NOTES) {
                    *error = "Too many GOGO changes";
                    return false;
                }
                size_t capacity = p->gogo_capacity ? p->gogo_capacity * 2 : 16;
                TjaGogoChange* changes = chart_resize(
                    chart->gogo_changes,
                    chart->gogo_change_count * sizeof(TjaGogoChange),
                    capacity * sizeof(TjaGogoChange));
                if(!changes) {
                    *error = "Not enough memory";
                    return false;
                }
                chart->gogo_changes = changes;
                p->gogo_capacity = capacity;
            }
            chart->gogo_changes[chart->gogo_change_count++] =
                (TjaGogoChange){.time_ms = (int32_t)lroundf(p->time_ms), .active = active};
        }
    } else if(c->kind == CommandBarline)
        p->barline = c->value != 0;
    else if(c->kind == CommandMeasure)
        p->beats = c->value;
    else if(c->kind == CommandScroll)
        p->scroll = c->value;
    else
        p->time_ms += c->value * 1000.0f;
    if(fabsf(p->time_ms) > MAX_TIME_MS) {
        *error = "Chart exceeds one hour";
        return false;
    }
    return true;
}

static bool finish_measure(Parser* p, TjaChart* chart, const char** error) {
    size_t command = 0;
    size_t slots = p->slot_count ? p->slot_count : 1;
    for(size_t i = 0; i <= slots; i++) {
        while(command < p->command_count && p->commands[command].slot == i) {
            if(!apply_measure_command(p, chart, &p->commands[command++], error)) return false;
        }
        if(fabsf(p->time_ms) > MAX_TIME_MS) {
            *error = "Chart exceeds one hour";
            return false;
        }
        if(i == slots) break;
        if(i == 0) {
            if(chart->bar_count == MAX_NOTES) {
                *error = "Chart exceeds 8192 measures";
                return false;
            }
            if(chart->bar_count == p->bar_capacity) {
                size_t capacity = p->bar_capacity ? p->bar_capacity * 2 : 32;
                TjaBar* bars = chart_resize(
                    chart->bars, chart->bar_count * sizeof(TjaBar), capacity * sizeof(TjaBar));
                if(!bars) {
                    *error = "Not enough memory";
                    return false;
                }
                chart->bars = bars;
                p->bar_capacity = capacity;
            }
            float speed = p->bpm / TAIKO_SCROLL_REFERENCE_BPM * p->scroll;
            if(!isfinite(speed) || speed == 0) {
                *error = "Scroll speed out of range";
                return false;
            }
            chart->bars[chart->bar_count++] = (TjaBar){
                .time_ms = (int32_t)lroundf(p->time_ms), .scroll_speed = p->barline ? speed : 0};
            if(p->barline && fabsf(speed) < chart->min_bar_speed)
                chart->min_bar_speed = fabsf(speed);
        }
        if(p->slot_count && !add_note(p, chart, p->slots[i], error)) return false;
        p->time_ms += 60000.0f / p->bpm * p->beats / slots;
    }
    p->slot_count = 0;
    p->command_count = 0;
    return true;
}

static bool command(Parser* p, char* line, const char** error) {
    char* argument = line;
    while(*argument && !isspace((unsigned char)*argument))
        argument++;
    if(*argument) *argument++ = '\0';
    argument = trim(argument);
    MeasureCommand c = {.slot = p->slot_count};
    if(strcmp(line, "#BPMCHANGE") == 0) {
        c.kind = CommandBpm;
        if(!number(argument, &c.value) || c.value < 1 || c.value > 10000) goto invalid;
    } else if(strcmp(line, "#SCROLL") == 0) {
        c.kind = CommandScroll;
        if(!number(argument, &c.value) || c.value == 0) {
            *error = "Invalid SCROLL";
            return false;
        }
    } else if(strcmp(line, "#MEASURE") == 0) {
        c.kind = CommandMeasure;
        char* slash = strchr(argument, '/');
        float numerator, denominator;
        if(!slash) goto invalid;
        *slash++ = '\0';
        if(!number(argument, &numerator) || !number(slash, &denominator) || numerator <= 0 ||
           denominator <= 0)
            goto invalid;
        c.value = 4.0f * numerator / denominator;
        if(!isfinite(c.value) || c.value > 64) goto invalid;
    } else if(strcmp(line, "#DELAY") == 0) {
        c.kind = CommandDelay;
        /* Negative delay would make the event list non-monotonic. */
        if(!number(argument, &c.value) || c.value < 0 || c.value > 3600) goto invalid;
    } else if(strcmp(line, "#GOGOSTART") == 0 || strcmp(line, "#GOGOEND") == 0) {
        c.kind = CommandGogo;
        c.value = strcmp(line, "#GOGOSTART") == 0 ? 1 : 0;
    } else if(strcmp(line, "#BARLINEON") == 0 || strcmp(line, "#BARLINEOFF") == 0) {
        c.kind = CommandBarline;
        c.value = strcmp(line, "#BARLINEON") == 0 ? 1 : 0;
    } else if(strcmp(line, "#BRANCHSTART") == 0 || strcmp(line, "#NEXTSONG") == 0) {
        *error = "Branches / medleys unsupported";
        return false;
    } else {
        /* Other unsupported visual commands do not affect timing. */
        return true;
    }
    if(p->command_count == MAX_COMMANDS) {
        *error = "Too many measure commands";
        return false;
    }
    p->commands[p->command_count++] = c;
    return true;
invalid:
    *error = "Invalid timing command";
    return false;
}

static bool build_hit_runs(TjaChart* chart, uint8_t speed_tenths, const char** error) {
    size_t capacity = 0, change = 0;
    double bpm = chart->initial_bpm;
    const double minimum = (double)1000 * speed_tenths / (TAIKO_ROLL_MAX_HZ * 10);
    double last_hit = 0;
    bool have_hit = false;
    for(size_t i = 0; i < chart->roll_count; i++) {
        TjaRoll* roll = &chart->rolls[i];
        double time = roll->time_ms, phase = 0;
        roll->first_run = chart->hit_run_count;
        roll->run_count = 0;
        roll->hit_count = 0;
        while(change < chart->bpm_change_count && chart->bpm_changes[change].time_ms <= time)
            bpm = chart->bpm_changes[change++].bpm;
        while(time < roll->end_ms) {
            double boundary = roll->end_ms;
            if(change < chart->bpm_change_count && chart->bpm_changes[change].time_ms < boundary)
                boundary = chart->bpm_changes[change].time_ms;
            double interval = 7500 / bpm;
            if(interval < minimum) interval = minimum;
            double first = time + phase * interval;
            /* Also bound short gaps across tempo changes and adjacent rolls. */
            if(have_hit && first < last_hit + minimum) first = last_hit + minimum;
            uint32_t count = first < boundary ?
                                 (uint32_t)ceil((boundary - first) / interval - (double)1e-7f) :
                                 0;
            if(roll->balloon_count && count > roll->balloon_count - roll->hit_count)
                count = roll->balloon_count - roll->hit_count;
            /* Onsets use integer milliseconds everywhere; never strike at note 8. */
            while(count && lround(first + (count - 1) * interval) >= roll->end_ms)
                count--;
            if(count) {
                if(chart->hit_run_count == capacity) {
                    if(capacity == MAX_NOTES) {
                        *error = "Too many roll tempo segments";
                        return false;
                    }
                    capacity = capacity ? capacity * 2 : 16;
                    TjaHitRun* runs = chart_resize(
                        chart->hit_runs,
                        chart->hit_run_count * sizeof(TjaHitRun),
                        capacity * sizeof(TjaHitRun));
                    if(!runs) {
                        *error = "Not enough memory";
                        return false;
                    }
                    chart->hit_runs = runs;
                }
                chart->hit_runs[chart->hit_run_count++] =
                    (TjaHitRun){.first_ms = first, .interval_ms = interval, .count = count};
                roll->run_count++;
                roll->hit_count += count;
                last_hit = first + (count - 1) * interval;
                have_hit = true;
            }
            if(roll->balloon_count && roll->hit_count == roll->balloon_count) break;
            phase = (first + count * interval - boundary) / interval;
            time = boundary;
            while(change < chart->bpm_change_count && chart->bpm_changes[change].time_ms <= time)
                bpm = chart->bpm_changes[change++].bpm;
        }
    }
    return true;
}

bool tja_chart_set_roll_speed(TjaChart* chart, uint8_t speed_tenths, const char** error) {
    *error = NULL;
    if(speed_tenths < 5 || speed_tenths > 20) {
        *error = "Invalid playback speed";
        return false;
    }
    if(chart->roll_speed_tenths == speed_tenths) return true;
    TjaChart replacement = *chart;
    replacement.hit_runs = NULL;
    replacement.hit_run_count = 0;
    replacement.rolls = NULL;
    if(chart->roll_count) {
        replacement.rolls = chart_alloc(chart->roll_count * sizeof(TjaRoll));
        if(!replacement.rolls) {
            *error = "Not enough memory";
            return false;
        }
        memcpy(replacement.rolls, chart->rolls, chart->roll_count * sizeof(TjaRoll));
    }
    if(!build_hit_runs(&replacement, speed_tenths, error)) {
        free(replacement.rolls);
        free(replacement.hit_runs);
        return false;
    }
    free(chart->rolls);
    free(chart->hit_runs);
    chart->rolls = replacement.rolls;
    chart->hit_runs = replacement.hit_runs;
    chart->hit_run_count = replacement.hit_run_count;
    chart->roll_speed_tenths = speed_tenths;
    return true;
}

static bool parse_selected(
    TjaReadLine reader,
    void* context,
    int requested,
    TjaChart* chart,
    const char** error) {
    memset(chart, 0, sizeof(*chart));
    *error = "Invalid TJA";
    Parser* p = chart_alloc(sizeof(Parser));
    char* buffer = chart_alloc(2048);
    if(!p || !buffer) {
        free(p);
        free(buffer);
        *error = "Not enough memory";
        return false;
    }
    memset(p, 0, sizeof(*p));
    p->bpm = 120;
    chart->roll_speed_tenths = 10;
    p->scroll = 1;
    p->barline = true;
    chart->min_note_speed = 1;
    chart->min_bar_speed = 1;
    p->beats = 4;
    chart->course_id = TjaCourseOni;
    snprintf(chart->course, sizeof(chart->course), "%s", tja_course_name(chart->course_id));
    bool started = false, success = false;
    bool skipping = false;
    bool first_line = true;
    int result;
    while((result = reader(context, buffer, 2048)) > 0) {
        char* line = clean_line(buffer, &first_line);
        if(!*line) continue;
        if(skipping) {
            if(strcmp(line, "#END") == 0) skipping = false;
            continue;
        }
        if(!started) {
            if(is_start(line)) {
                if(requested >= 0 && (int)chart->course_id != requested) {
                    skipping = true;
                    continue;
                }
                if(!*chart->wave) {
                    *error = "Missing WAVE in TJA";
                    break;
                }
                chart->initial_bpm = p->bpm;
                started = true;
            } else {
                char* colon = strchr(line, ':');
                if(!colon) continue;
                *colon++ = '\0';
                char* value = trim(colon);
                line = trim(line);
                if(strcmp(line, "TITLE") == 0)
                    snprintf(chart->title, sizeof(chart->title), "%s", value);
                else if(strcmp(line, "COURSE") == 0) {
                    p->balloons[0] = '\0';
                    p->next_balloon = p->balloons;
                    if(!read_course(value, &chart->course_id)) {
                        *error = "Unsupported COURSE";
                        break;
                    }
                    snprintf(
                        chart->course,
                        sizeof(chart->course),
                        "%s",
                        tja_course_name(chart->course_id));
                } else if(strcmp(line, "BALLOON") == 0) {
                    strcpy(p->balloons, value); /* Bounded by the line reader. */
                    p->next_balloon = p->balloons;
                } else if(strcmp(line, "WAVE") == 0) {
                    if(strlen(value) >= sizeof(chart->wave)) {
                        *error = "WAVE path is too long";
                        break;
                    }
                    strcpy(chart->wave, value);
                } else if(strcmp(line, "BPM") == 0) {
                    if(!number(value, &p->bpm) || p->bpm < 1 || p->bpm > 10000) {
                        *error = "Invalid BPM";
                        break;
                    }
                } else if(strcmp(line, "OFFSET") == 0) {
                    float offset;
                    if(!number(value, &offset) || fabsf(offset) > 3600) {
                        *error = "Invalid OFFSET";
                        break;
                    }
                    p->time_ms = -offset * 1000.0f;
                }
            }
        } else if(strcmp(line, "#END") == 0) {
            if(p->slot_count) {
                *error = "Missing final measure comma";
                break;
            }
            bool valid = true;
            for(size_t i = 0; i < p->command_count; i++) {
                if(!apply_measure_command(p, chart, &p->commands[i], error)) {
                    valid = false;
                    break;
                }
            }
            if(!valid) break;
            chart->end_ms = (int32_t)lroundf(p->time_ms);
            if(p->active_roll) chart->rolls[p->active_roll - 1].end_ms = chart->end_ms;
            success = build_hit_runs(chart, 10, error);
            break;
        } else if(*line == '#') {
            if(!command(p, line, error)) break;
        } else {
            bool valid = true;
            for(char* c = line; *c; c++) {
                if(*c == ',') {
                    if(!finish_measure(p, chart, error)) {
                        valid = false;
                        break;
                    }
                } else if(*c >= '0' && *c <= '9') {
                    if(p->slot_count == MAX_SLOTS) {
                        *error = "Measure exceeds 2048 slots";
                        valid = false;
                        break;
                    }
                    p->slots[p->slot_count++] = *c;
                } else if(!isspace((unsigned char)*c)) {
                    *error = "Unsupported note character";
                    valid = false;
                    break;
                }
            }
            if(!valid) break;
        }
    }
    if(result < 0)
        *error = "TJA read error / line too long";
    else if(result == 0)
        *error = "Missing #START or #END";
    free(buffer);
    free(p);
    if(success) *error = NULL;
    return success;
}

bool tja_parse(TjaReadLine reader, void* context, TjaChart* chart, const char** error) {
    return parse_selected(reader, context, -1, chart, error);
}

bool tja_parse_course(
    TjaReadLine reader,
    void* context,
    TjaCourse course,
    TjaChart* chart,
    const char** error) {
    return parse_selected(reader, context, course, chart, error);
}

void tja_chart_free(TjaChart* chart) {
    free(chart->notes);
    free(chart->bpm_changes);
    free(chart->rolls);
    free(chart->hit_runs);
    free(chart->gogo_changes);
    free(chart->bars);
    memset(chart, 0, sizeof(*chart));
}

float tja_chart_bpm_at(const TjaChart* chart, uint32_t position_ms) {
    size_t low = 0, high = chart->bpm_change_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(chart->bpm_changes[mid].time_ms <= (int64_t)position_ms)
            low = mid + 1;
        else
            high = mid;
    }
    return low ? chart->bpm_changes[low - 1].bpm : chart->initial_bpm;
}

bool tja_chart_gogo_at(const TjaChart* chart, uint32_t position_ms) {
    if((int64_t)position_ms >= chart->end_ms) return false;
    size_t low = 0, high = chart->gogo_change_count;
    while(low < high) {
        size_t mid = low + (high - low) / 2;
        if(chart->gogo_changes[mid].time_ms <= (int64_t)position_ms)
            low = mid + 1;
        else
            high = mid;
    }
    return low && chart->gogo_changes[low - 1].active;
}

uint32_t tja_chart_seek_bars(
    const TjaChart* chart,
    uint32_t position_ms,
    uint32_t duration_ms,
    bool forward,
    uint32_t count) {
    int64_t target = position_ms;
    /* Binary search each step also skips starts rounded to the same millisecond.
     * The menu limits count to ten; no per-frame or audio-worker work is added. */
    for(uint32_t step = 0; step < count; step++) {
        size_t low = 0, high = chart->bar_count;
        while(low < high) {
            size_t mid = low + (high - low) / 2;
            if(chart->bars[mid].time_ms < target ||
               (forward && chart->bars[mid].time_ms == target))
                low = mid + 1;
            else
                high = mid;
        }
        if(forward) {
            if(low == chart->bar_count) return duration_ms;
            target = chart->bars[low].time_ms;
        } else {
            if(!low) return 0;
            target = chart->bars[low - 1].time_ms;
        }
        if(target < 0) return 0;
        if(target >= duration_ms) return duration_ms;
    }
    return target > duration_ms ? duration_ms : (uint32_t)target;
}
