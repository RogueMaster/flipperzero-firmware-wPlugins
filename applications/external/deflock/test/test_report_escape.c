// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// B12: report field escaping vs injection payloads. A detected SSID / BLE name
// is up to 32 bytes of attacker-influenced data written into CSV / JSON / KML /
// Markdown reports; these tests prove a hostile value can't break out of its
// field (add CSV columns, close a JSON string, inject KML elements, or corrupt a
// Markdown table). Now testable because R8 pulled the escapers into report_escape.
#include "report_escape.h"
#include "test.h"

#include <string.h>
#include <stdio.h>

void suite_report_escape(void) {
    printf("[report_escape]\n");
    char out[128];

    // --- csv_field_escape (RFC-4180 field integrity) -----------------------
    csv_field_escape("hello", out, sizeof(out));
    CHECK_STR_EQ(out, "hello"); // no special chars -> unquoted
    csv_field_escape("a,b", out, sizeof(out));
    CHECK_STR_EQ(out, "\"a,b\""); // comma -> quoted (stays one field)
    csv_field_escape("a\"b", out, sizeof(out));
    CHECK_STR_EQ(out, "\"a\"\"b\""); // embedded quote doubled
    csv_field_escape("line1\nline2", out, sizeof(out));
    CHECK_STR_EQ(out, "\"line1\nline2\""); // newline -> quoted
    csv_field_escape("Free,WiFi,Here", out, sizeof(out)); // injection: no extra columns
    CHECK_STR_EQ(out, "\"Free,WiFi,Here\"");
    // Truncation stays well-formed (properly closed quote inside the buffer).
    char small[6];
    csv_field_escape("aaaa,bbbb", small, sizeof(small));
    CHECK_STR_EQ(small, "\"aaa\"");

    // --- json_escape (string content) --------------------------------------
    json_escape("hello", out, sizeof(out));
    CHECK_STR_EQ(out, "hello");
    json_escape("a\"b", out, sizeof(out));
    CHECK_STR_EQ(out, "a\\\"b"); // double-quote escaped
    json_escape("a\\b", out, sizeof(out));
    CHECK_STR_EQ(out, "a\\\\b"); // backslash doubled
    json_escape("l1\nl2\t!", out, sizeof(out));
    CHECK_STR_EQ(out, "l1\\nl2\\t!");
    json_escape("\x01", out, sizeof(out));
    CHECK_STR_EQ(out, "\\u0001"); // control -> \u00xx
    // Injection: a payload trying to close the string + inject a key is neutralised
    // (every quote escaped) -- CHECK_STR_EQ proves the exact escaped form.
    json_escape("\"},{\"x\":\"", out, sizeof(out));
    CHECK_STR_EQ(out, "\\\"},{\\\"x\\\":\\\"");

    // --- md_escape (table cell) --------------------------------------------
    md_escape("hello", out, sizeof(out));
    CHECK_STR_EQ(out, "hello");
    md_escape("a|b", out, sizeof(out));
    CHECK_STR_EQ(out, "a\\|b"); // pipe -> \| (no extra column)
    md_escape("co`de", out, sizeof(out));
    CHECK_STR_EQ(out, "co'de"); // backtick -> '
    md_escape("a\nb", out, sizeof(out));
    CHECK_STR_EQ(out, "a b"); // newline -> space (row not split)
    md_escape("x | evil | y", out, sizeof(out)); // injection: pipes escaped
    CHECK_STR_EQ(out, "x \\| evil \\| y");

    // --- xml_escape (KML text/attr) ----------------------------------------
    xml_escape("hello", out, sizeof(out));
    CHECK_STR_EQ(out, "hello");
    xml_escape("a&b", out, sizeof(out));
    CHECK_STR_EQ(out, "a&amp;b");
    xml_escape("<tag>", out, sizeof(out));
    CHECK_STR_EQ(out, "&lt;tag&gt;");
    xml_escape("q\"'a", out, sizeof(out));
    CHECK_STR_EQ(out, "q&quot;&apos;a");
    // Injection: a payload trying to inject KML elements is fully entity-encoded,
    // so NO raw angle brackets survive.
    xml_escape("</name><evil>", out, sizeof(out));
    CHECK_STR_EQ(out, "&lt;/name&gt;&lt;evil&gt;");
    CHECK(strchr(out, '<') == NULL && strchr(out, '>') == NULL);
}
