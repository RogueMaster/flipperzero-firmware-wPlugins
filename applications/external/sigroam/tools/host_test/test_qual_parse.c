#include "sr_test.h"

#include "sr_parse_marauder.h"
#include "sr_model.h"
#include "sr_bloom.h"
#include "sr_rawlog.h"
#include "sr_types.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static SrBloom g_bloom;
static SrRawLog g_rawlog;

static const char kQualOk[] = "Qual: gga=12 ggafix=11 drop=3 net=142 sd=1 sats=8 topd=1";

static SrParseResult feed(SrParser* p, const char* line, size_t n, SrEvent* ev) {
    memset(ev, 0, sizeof(*ev));
    return sr_codec_marauder.feed_line(p, line, n, ev);
}

int test_qual_parse_run(void);

int test_qual_parse_run(void) {
    SrParser parser;
    SrEvent ev;
    SrModel m;
    SrFirmwareInfo fw;

    sr_test_failures = 0;

    CHECK((unsigned)SrEventSess == 9u);
    CHECK((unsigned)SrEventQual == 10u);
    CHECK(sizeof(SrQualInfo) == 20u);
    CHECK(sizeof(SrSessInfo) == 12u);

    memset(&parser, 0, sizeof(parser));
    CHECK(feed(&parser, kQualOk, strlen(kQualOk), &ev) == SrParseOk);
    CHECK(ev.kind == SrEventQual);
    CHECK(ev.u.qual.gga == 12u);
    CHECK(ev.u.qual.ggafix == 11u);
    CHECK(ev.u.qual.drop == 3u);
    CHECK(ev.u.qual.net == 142u);
    CHECK(ev.u.qual.sd == 1u);
    CHECK(ev.u.qual.sats == 8u);
    CHECK(ev.u.qual.topd == 1u);
    CHECK(parser.cmdack.rev == 0u);

    memset(&fw, 0, sizeof(fw));
    CHECK(sr_codec_marauder.probe_line(kQualOk, &fw) == false);
    CHECK(fw.diag_seen == false);

    /* zeros are a measurement, not "never seen". */
    {
        static const char kZero[] = "Qual: gga=0 ggafix=0 drop=0 net=0 sd=0 sats=0 topd=0";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kZero, strlen(kZero), &ev) == SrParseOk);
        CHECK(ev.kind == SrEventQual);
        CHECK(ev.u.qual.gga == 0u);
        CHECK(ev.u.qual.sd == 0u);
    }

    /* Named: qual_parse rejects trailing bytes (p!=len). NC half-B (1) lands here. */
    {
        char junk[128];
        size_t n = strlen(kQualOk);
        CHECK(n + 2u < sizeof(junk));
        memcpy(junk, kQualOk, n);
        junk[n] = ' ';
        junk[n + 1u] = 'x';
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, junk, n + 2u, &ev) == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
    }

    /* renamed field is not Qual */
    {
        static const char kBad[] = "Qual: ggx=12 ggafix=11 drop=3 net=142 sd=1 sats=8 topd=1";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kBad, strlen(kBad), &ev) == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
    }

    /* sd/sats/topd over 255 rejected */
    {
        static const char kSd[] = "Qual: gga=1 ggafix=1 drop=0 net=0 sd=256 sats=0 topd=0";
        memset(&parser, 0, sizeof(parser));
        CHECK(feed(&parser, kSd, strlen(kSd), &ev) == SrParseUnknown);
        CHECK(ev.kind == SrEventUnknown);
    }

    /* A missing Qual: line is unknown, not zero: never-seen stays qual_rev==0. */
    sr_bloom_init(&g_bloom);
    memset(&g_rawlog, 0, sizeof(g_rawlog));
    sr_model_init(&m, &g_bloom, &g_rawlog);
    CHECK(m.qual_rev == 0u);
    CHECK(m.qual.gga == 0u);
    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventSess;
    ev.u.sess.ap = 1u;
    CHECK(sr_model_apply(&m, &ev, 10u) == true);
    CHECK(m.qual_rev == 0u);
    CHECK(m.unknown_lines == 0u);

    memset(&ev, 0, sizeof(ev));
    ev.kind = SrEventQual;
    ev.u.qual.gga = 12u;
    ev.u.qual.ggafix = 11u;
    ev.u.qual.drop = 3u;
    ev.u.qual.net = 142u;
    ev.u.qual.sd = 1u;
    ev.u.qual.sats = 8u;
    ev.u.qual.topd = 1u;
    CHECK(sr_model_apply(&m, &ev, 20u) == true);
    CHECK(m.qual_rev == 1u);
    CHECK(m.qual.gga == 12u);
    CHECK(m.qual.net == 142u);
    CHECK(m.unknown_lines == 0u);

    /* reset_session keeps qual / qual_rev */
    m.ap_wifi = 9u;
    sr_model_reset_session(&m, false);
    CHECK(m.qual_rev == 1u);
    CHECK(m.qual.gga == 12u);
    CHECK(m.ap_wifi == 0u);

    fprintf(stderr, "qual_parse: ok\n");
    return sr_test_failures;
}
