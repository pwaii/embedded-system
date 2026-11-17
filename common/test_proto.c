/* Host-side tests for proto.c — run with: make -C common test */
#include "proto.h"

#include <stdio.h>
#include <string.h>

static int fails = 0, passes = 0;

#define CHECK(cond) do { if (cond) passes++; else { fails++; \
    printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* parse -> format must reproduce the exact line */
static void roundtrip(const char *line)
{
    proto_msg_t m;
    char out[PROTO_MAX_LINE + 8];
    char want[PROTO_MAX_LINE + 8];
    proto_result_t r = proto_parse_line(line, &m);
    int n;
    snprintf(want, sizeof want, "%s\n", line);
    if (r != PP_OK) { fails++; printf("FAIL roundtrip parse (%d): %s\n", r, line); return; }
    n = proto_format(&m, out, sizeof out);
    if (n < 0 || strcmp(out, want) != 0) { fails++; printf("FAIL roundtrip format: %s -> %s", line, n < 0 ? "(err)\n" : out); return; }
    passes++;
}

static proto_result_t P(const char *line) { proto_msg_t m; return proto_parse_line(line, &m); }

int main(void)
{
    proto_msg_t m;

    /* every example in docs/protocol.md */
    roundtrip("HELLO,STM,1.0");
    roundtrip("HELLO,ESP,1.0");
    roundtrip("STATE,-,SETUP,4,60,0,0,60000");
    roundtrip("STATE,1a2b3c4d,RUN,4,60,7,3,-2100");
    roundtrip("STATE,9f03c2aa,OVER,4,20,9,1,11900");
    roundtrip("START,1a2b3c4d,4,60");
    roundtrip("TURN,1a2b3c4d,7,3,62100,2100");
    roundtrip("END,1a2b3c4d,24,USER");
    roundtrip("END,1a2b3c4d,3,RESET");
    roundtrip("ACK,CFG");
    roundtrip("ACK,CMD");
    roundtrip("ACK,TURN,7");
    roundtrip("ERR,CFG,BUSY");
    roundtrip("ERR,CMD,RANGE");
    roundtrip("CFG,3,90");
    roundtrip("CMD,PAUSE");
    roundtrip("CMD,START");
    roundtrip("CMD,RESET");
    roundtrip("WIFI,1");
    roundtrip("WIFI,0");

    /* worst-case lengths still fit in 64 bytes */
    roundtrip("STATE,ffffffff,PAUSE,6,300,999,6,-9999999");
    roundtrip("TURN,ffffffff,999,6,9999999,9999999");

    /* CRLF tolerated, values decoded */
    CHECK(proto_parse_line("STATE,1a2b3c4d,RUN,4,60,7,3,-2100\r\n", &m) == PP_OK);
    CHECK(m.type == PM_STATE && m.u.state.has_game && m.u.state.game_id == 0x1a2b3c4d);
    CHECK(m.u.state.state == PS_RUN && m.u.state.remaining_ms == -2100 && m.u.state.player == 3);

    CHECK(proto_parse_line("TURN,0000000a,1,2,17250,0", &m) == PP_OK);
    CHECK(m.u.turn.game_id == 10 && m.u.turn.duration_ms == 17250 && m.u.turn.overtime_ms == 0);

    /* framing errors */
    CHECK(P("") == PP_EMPTY);
    CHECK(P("\r\n") == PP_EMPTY);
    CHECK(P("0123456789012345678901234567890123456789012345678901234567890123") == PP_TOO_LONG);
    CHECK(P("FOO,1") == PP_UNKNOWN);
    CHECK(P("cfg,3,90") == PP_UNKNOWN);          /* type is case-sensitive */

    /* parse errors */
    CHECK(P("CFG,3") == PP_PARSE);
    CHECK(P("CFG,3,90,1") == PP_PARSE);
    CHECK(P("CFG,3,") == PP_PARSE);
    CHECK(P("CFG,x,90") == PP_PARSE);
    CHECK(P("CFG,3, 90") == PP_PARSE);
    CHECK(P("TURN,1A2B3C4D,1,1,100,0") == PP_PARSE);   /* game_id must be lower-case */
    CHECK(P("TURN,1a2b3c,1,1,100,0") == PP_PARSE);     /* game_id must be 8 chars */
    CHECK(P("ACK,TURN") == PP_PARSE);
    CHECK(P("ACK,CFG,3") == PP_PARSE);
    CHECK(P("STATE,-,WAIT,4,60,0,0,0") == PP_PARSE);

    /* range errors */
    CHECK(P("CFG,7,90") == PP_RANGE);
    CHECK(P("CFG,1,90") == PP_RANGE);
    CHECK(P("CFG,3,5") == PP_RANGE);
    CHECK(P("CFG,3,301") == PP_RANGE);
    CHECK(P("CFG,3,92") == PP_RANGE);            /* multiple of 5 */
    CHECK(P("CMD,JUMP") == PP_RANGE);
    CHECK(P("WIFI,2") == PP_RANGE);
    CHECK(P("TURN,1a2b3c4d,1,1,100,200") == PP_RANGE);  /* overtime > duration */
    CHECK(P("STATE,-,RUN,3,60,1,4,1000") == PP_RANGE);  /* player > players */

    /* error mapping for replies */
    CHECK(proto_err_from_result(PP_UNKNOWN) == PE_UNKNOWN);
    CHECK(proto_err_from_result(PP_RANGE) == PE_RANGE);
    CHECK(proto_err_from_result(PP_PARSE) == PE_PARSE);

    /* line assembler: normal, CRLF, overflow + resync */
    {
        proto_rx_t rx;
        const char *stream = "CMD,PAUSE\r\nXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX\nWIFI,1\n";
        char got[4][PROTO_MAX_LINE + 1];
        int lines = 0;
        proto_rx_init(&rx);
        for (const char *p = stream; *p; p++)
            if (proto_rx_push(&rx, *p) && lines < 4) strcpy(got[lines++], rx.buf);
        CHECK(lines == 2);
        CHECK(lines >= 1 && strcmp(got[0], "CMD,PAUSE") == 0);
        CHECK(lines >= 2 && strcmp(got[1], "WIFI,1") == 0);
    }
    /* exactly 63 content bytes is legal (64 with '\n'), 64 is not */
    {
        proto_rx_t rx;
        int ok63 = 0, ok64 = 0;
        proto_rx_init(&rx);
        for (int i = 0; i < 63; i++) proto_rx_push(&rx, 'A');
        ok63 = proto_rx_push(&rx, '\n') && strlen(rx.buf) == 63;
        for (int i = 0; i < 64; i++) proto_rx_push(&rx, 'A');
        ok64 = proto_rx_push(&rx, '\n');
        CHECK(ok63);
        CHECK(!ok64);
    }

    printf("%d passed, %d failed\n", passes, fails);
    return fails ? 1 : 0;
}
