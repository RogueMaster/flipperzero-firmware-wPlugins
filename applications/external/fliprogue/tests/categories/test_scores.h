#pragma once

#include "score_data.h"

static void test_v12_score_format_and_orb_sorting(void) {
    ScoreEntry scores[SCORE_CAP] = {0};
    uint8_t count = 0;
    ScoreEntry plain = {0, 3, 2, 0, FR_CLASS_RANGER, 999, "Killed by rat."};
    ScoreEntry orb = {0, 18, 2, 1, FR_CLASS_WARRIOR, 150, "Killed by rat."};
    ScoreEntry rich_orb = {1, 18, 6, 1, FR_CLASS_MAGE, 200, "Escaped with the Orb."};

    fr_score_insert(scores, &count, &plain);
    fr_score_insert(scores, &count, &orb);
    fr_score_insert(scores, &count, &rich_orb);

    assert(count == 3);
    assert(scores[0].has_orb);
    assert(scores[0].gold == 200);
    assert(scores[1].has_orb);
    assert(scores[1].gold == 150);
    assert(!scores[2].has_orb);

    char line[32];
    char log_line[48];
    fr_score_format_main(&scores[1], 2, line, sizeof(line));
    fr_score_format_log(&scores[1], 2, log_line, sizeof(log_line));
    assert(strcmp(line, "2. & W F18 L2 150g") == 0);
    assert(strcmp(log_line, "   Killed by rat.") == 0);

    fr_score_format_log(&scores[0], 10, log_line, sizeof(log_line));
    assert(strncmp(log_line, "    ", 4) == 0);

    ScoreEntry long_log = {0, 3, 2, 0, FR_CLASS_RANGER, 10, "A. B. C."};
    fr_score_format_log(&long_log, 1, log_line, sizeof(log_line));
    assert(strcmp(log_line, "   B. C.") == 0);

    ScoreEntry parsed = {0};
    assert(fr_score_parse_main("10. & M F18 L6 200g", &parsed));
    fr_score_parse_log("    Escaped with the Orb.", &parsed);
    assert(parsed.has_orb);
    assert(parsed.class_id == FR_CLASS_MAGE);
    assert(parsed.floor == 18);
    assert(parsed.level == 6);
    assert(parsed.gold == 200);
    assert(strcmp(parsed.log, "Escaped with the Orb.") == 0);

    memset(&parsed, 0, sizeof(parsed));
    assert(fr_score_parse_main("1.   R F1 L1 4g", &parsed));
    fr_score_parse_log("   You miss Rat. Killed by Rat.", &parsed);
    assert(!parsed.has_orb);
    assert(parsed.class_id == FR_CLASS_RANGER);
    assert(parsed.floor == 1);
    assert(parsed.level == 1);
    assert(parsed.gold == 4);
    assert(strcmp(parsed.log, "You miss Rat. Killed by Rat.") == 0);
}
