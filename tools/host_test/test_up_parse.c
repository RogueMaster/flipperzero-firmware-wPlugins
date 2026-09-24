#include "sr_test.h"

#include "sr_parse_marauder.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"
#include "sr_types.h"

#include <stdio.h>
#include <string.h>

static SrBloom g_bloom;
static SrRawLog g_rawlog;

static const char kUpOk[] =
    "Up: up_q=3 up_last_trans=abc-XYZ.1_2 up_disc_gps=0 up_reason=STARTED";

static SrParseResult feed(SrParser* p, const char* line, size_t n, SrEvent* ev) {
    memset(ev, 0, sizeof(*ev));
    return sr_codec_marauder.feed_line(p, line, n, ev);
}

int test_up_parse_run(void);

int test_up_parse_run(void) {
    SrParser parser;
    SrEvent ev;
    SrModel m;
    SrFirmwareInfo fw;

    sr_test_failures = 0;

    CHECK((unsigned)SrEventRadio == 11u);
    CHECK((unsigned)SrEventUp == 12u);
    CHECK((unsigned)SrEventRank == 13u);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, kUpOk, strlen(kUpOk), &ev) == SrParseOk);
    CHECK(ev.kind == SrEventUp);
    CHECK(ev.u.up.q == 3u);
    CHECK(ev.u.up.disc_gps == 0u);
    CHECK(strcmp(ev.u.up.last_trans, "abc-XYZ.1_2") == 0);
    CHECK(strcmp(ev.u.up.reason, "STARTED") == 0);
    CHECK(parser.cmdack.rev == 0u);

    memset(&fw, 0, sizeof(fw));
    CHECK(sr_codec_marauder.probe_line(kUpOk, &fw) == false);
    CHECK(fw.diag_seen == false);

    {
        static const char kEmpty[] =
            "Up: up_q=0 up_last_trans=- up_disc_gps=0 up_reason=-";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kEmpty, strlen(kEmpty), &ev) == SrParseOk);
        CHECK(ev.kind == SrEventUp);
        CHECK(ev.u.up.q == 0u);
        CHECK(ev.u.up.last_trans[0] == '-' && ev.u.up.last_trans[1] == '\0');
        CHECK(ev.u.up.reason[0] == '-' && ev.u.up.reason[1] == '\0');
    }

    {
        char junk[160];
        size_t n = strlen(kUpOk);
        CHECK(n + 2u < sizeof(junk));
        memcpy(junk, kUpOk, n);
        junk[n] = ' ';
        junk[n + 1u] = 'x';
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, junk, n + 2u, &ev) == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
    }

    {
        static const char kBad[] =
            "Up: up_q=1 up_last_trans=has space up_disc_gps=0 up_reason=OK";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kBad, strlen(kBad), &ev) == SrParseUnknown);
    }

    sr_model_init(&m, &g_bloom, &g_rawlog);
    CHECK(m.up_rev == 0u);
    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, kUpOk, strlen(kUpOk), &ev) == SrParseOk);
    CHECK(sr_model_apply(&m, &ev, 1) == true);
    CHECK(m.up_rev == 1u);
    CHECK(m.up.q == 3u);
    CHECK(strcmp(m.up.reason, "STARTED") == 0);

    {
        static const char kEcho[] = "#uploadstatus";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kEcho, strlen(kEcho), &ev) == SrParseUnknown);
        CHECK(parser.cmdack.rev == 0u);
    }

    {
        static const char kEchoU[] = "#upload";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kEchoU, strlen(kEchoU), &ev) == SrParseUnknown);
        CHECK(parser.cmdack.rev == 0u);
    }

    {
        static const char kRank[] = "Rank: rank=100 month=7 wifi_gps=42";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kRank, strlen(kRank), &ev) == SrParseOk);
        CHECK(ev.kind == SrEventRank);
        CHECK(ev.u.rank.rank == 100u);
        CHECK(ev.u.rank.month == 7u);
        CHECK(ev.u.rank.wifi_gps == 42u);
        CHECK(parser.cmdack.rev == 0u);
    }

    {
        static const char kNeg[] = "Rank: rank=-1 month=7 wifi_gps=42";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kNeg, strlen(kNeg), &ev) == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
    }

    {
        static const char kOver[] = "Rank: rank=4294967296 month=7 wifi_gps=42";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kOver, strlen(kOver), &ev) == SrParseUnknown);
    }

    {
        static const char kShort[] = "Rank: rank=1 month=2";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kShort, strlen(kShort), &ev) == SrParseUnknown);
    }

    {
        static const char kTrail[] = "Rank: rank=1 month=2 wifi_gps=3 x";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kTrail, strlen(kTrail), &ev) == SrParseUnknown);
    }

    {
        static const char kMax[] =
            "Rank: rank=4294967295 month=4294967295 wifi_gps=4294967295";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kMax, strlen(kMax), &ev) == SrParseOk);
        CHECK(ev.u.rank.rank == 4294967295u);
        CHECK(ev.u.rank.month == 4294967295u);
        CHECK(ev.u.rank.wifi_gps == 4294967295u);
    }

    {
        static const char kRank[] = "Rank: rank=9 month=8 wifi_gps=7";
        memset(&parser, 0, sizeof(parser));
        CHECK(m.rank_rev == 0u);
        CHECK(m.up_rev == 1u);
        CHECK(feed(&parser, kRank, strlen(kRank), &ev) == SrParseOk);
        CHECK(sr_model_apply(&m, &ev, 2) == true);
        CHECK(m.rank_rev == 1u);
        CHECK(m.rank.rank == 9u);
        CHECK(m.rank.month == 8u);
        CHECK(m.rank.wifi_gps == 7u);
        CHECK(m.up_rev == 1u);
        CHECK(m.up.q == 3u);
    }

    return sr_test_failures;
}
