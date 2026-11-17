/* proto.c — reference implementation of docs/protocol.md v1.0 */
#include "proto.h"

#include <stdio.h>
#include <string.h>

#define MAX_FIELDS 9

/* ---------------------------------------------------------------- names */

static const char *const TYPE_NAMES[] = {
    "?", "HELLO", "STATE", "START", "TURN", "END", "CFG", "CMD", "WIFI", "ACK", "ERR"
};
static const char *const STATE_NAMES[] = { "SETUP", "READY", "RUN", "PAUSE", "OVER" };
static const char *const CMD_NAMES[]   = { "START", "PAUSE", "RESUME", "END", "RESET" };
static const char *const ERR_NAMES[]   = { "PARSE", "RANGE", "BUSY", "UNKNOWN" };
static const char *const END_NAMES[]   = { "USER", "RESET" };

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

const char *proto_type_name(proto_type_t t)
{
    return ((size_t)t < COUNT(TYPE_NAMES)) ? TYPE_NAMES[t] : "?";
}
const char *proto_state_name(proto_state_t s)
{
    return ((size_t)s < COUNT(STATE_NAMES)) ? STATE_NAMES[s] : "?";
}
const char *proto_cmd_name(proto_cmd_t c)
{
    return ((size_t)c < COUNT(CMD_NAMES)) ? CMD_NAMES[c] : "?";
}

static int lookup(const char *s, const char *const *table, size_t n)
{
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, table[i]) == 0) return (int)i;
    return -1;
}

static proto_type_t type_from_name(const char *s)
{
    int i = lookup(s, TYPE_NAMES, COUNT(TYPE_NAMES));
    return (i > 0) ? (proto_type_t)i : PM_INVALID;
}

proto_err_t proto_err_from_result(proto_result_t r)
{
    switch (r) {
    case PP_UNKNOWN: return PE_UNKNOWN;
    case PP_RANGE:   return PE_RANGE;
    default:         return PE_PARSE;
    }
}

/* ---------------------------------------------------------------- field parsing */

/* Strict signed decimal: optional '-', 1..8 digits, nothing else. */
static bool parse_int(const char *s, int32_t *out)
{
    bool neg = false;
    int32_t v = 0;
    int digits = 0;
    if (*s == '-') { neg = true; s++; }
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
        if (++digits > 8) return false;
        v = v * 10 + (*s - '0');
    }
    if (digits == 0) return false;
    *out = neg ? -v : v;
    return true;
}

static bool parse_uint_range(const char *s, int32_t lo, int32_t hi, int32_t *out, bool *range_ok)
{
    if (!parse_int(s, out)) return false;
    if (*out < lo || *out > hi) *range_ok = false;
    return true;
}

/* Exactly 8 lower-case hex chars. */
static bool parse_game_id(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    if (strlen(s) != 8) return false;
    for (int i = 0; i < 8; i++) {
        char c = s[i];
        uint32_t d;
        if (c >= '0' && c <= '9')      d = (uint32_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint32_t)(c - 'a' + 10);
        else return false;
        v = (v << 4) | d;
    }
    *out = v;
    return true;
}

static bool valid_limit(int32_t v) { return v >= 10 && v <= 300 && v % 5 == 0; }

/* ---------------------------------------------------------------- parse */

proto_result_t proto_parse_line(const char *line, proto_msg_t *out)
{
    char buf[PROTO_MAX_LINE + 1];
    char *f[MAX_FIELDS];
    int nf = 0;
    size_t len;
    bool ok = true;      /* range check result */
    int32_t a, b, c, d, e, g;

    memset(out, 0, sizeof(*out));
    out->type = PM_INVALID;

    len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) len--;
    if (len == 0) return PP_EMPTY;
    if (len + 1 > PROTO_MAX_LINE) return PP_TOO_LONG;   /* +1 for the '\n' on the wire */
    memcpy(buf, line, len);
    buf[len] = '\0';

    /* split on ',' — empty fields are kept and will fail parsing */
    f[nf++] = buf;
    for (char *p = buf; *p; p++) {
        if (*p == ',') {
            *p = '\0';
            if (nf == MAX_FIELDS) return PP_PARSE;
            f[nf++] = p + 1;
        }
    }

    out->type = type_from_name(f[0]);

#define NEED(n) do { if (nf != (n)) return PP_PARSE; } while (0)
#define INT(i, lo, hi, var) do { if (!parse_uint_range(f[i], (lo), (hi), &(var), &ok)) return PP_PARSE; } while (0)

    switch (out->type) {
    case PM_HELLO:
        NEED(3);
        if (strlen(f[1]) != 3 || strlen(f[2]) == 0 || strlen(f[2]) >= sizeof(out->u.hello.fw)) return PP_PARSE;
        if (strcmp(f[1], "STM") != 0 && strcmp(f[1], "ESP") != 0) return PP_RANGE;
        strcpy(out->u.hello.role, f[1]);
        strcpy(out->u.hello.fw, f[2]);
        return PP_OK;

    case PM_STATE: {
        int s;
        NEED(8);
        if (strcmp(f[1], "-") == 0) out->u.state.has_game = false;
        else if (parse_game_id(f[1], &out->u.state.game_id)) out->u.state.has_game = true;
        else return PP_PARSE;
        s = lookup(f[2], STATE_NAMES, COUNT(STATE_NAMES));
        if (s < 0) return PP_PARSE;
        INT(3, 2, 6, a);
        INT(4, 10, 300, b);
        INT(5, 0, 999, c);
        INT(6, 0, 6, d);
        INT(7, -9999999, 9999999, e);
        if (!valid_limit(b) || d > a) ok = false;
        out->u.state.state = (proto_state_t)s;
        out->u.state.players = (uint8_t)a;
        out->u.state.limit_s = (uint16_t)b;
        out->u.state.turn_no = (uint16_t)c;
        out->u.state.player = (uint8_t)d;
        out->u.state.remaining_ms = e;
        return ok ? PP_OK : PP_RANGE;
    }

    case PM_START:
        NEED(4);
        if (!parse_game_id(f[1], &out->u.start.game_id)) return PP_PARSE;
        INT(2, 2, 6, a);
        INT(3, 10, 300, b);
        if (!valid_limit(b)) ok = false;
        out->u.start.players = (uint8_t)a;
        out->u.start.limit_s = (uint16_t)b;
        return ok ? PP_OK : PP_RANGE;

    case PM_TURN:
        NEED(6);
        if (!parse_game_id(f[1], &out->u.turn.game_id)) return PP_PARSE;
        INT(2, 1, 999, a);
        INT(3, 1, 6, b);
        INT(4, 0, 9999999, c);
        INT(5, 0, 9999999, g);
        if (g > c) ok = false;                 /* overtime can't exceed duration */
        out->u.turn.turn_no = (uint16_t)a;
        out->u.turn.player = (uint8_t)b;
        out->u.turn.duration_ms = (uint32_t)c;
        out->u.turn.overtime_ms = (uint32_t)g;
        return ok ? PP_OK : PP_RANGE;

    case PM_END: {
        int r;
        NEED(4);
        if (!parse_game_id(f[1], &out->u.end.game_id)) return PP_PARSE;
        INT(2, 0, 999, a);
        r = lookup(f[3], END_NAMES, COUNT(END_NAMES));
        if (r < 0) return PP_PARSE;
        out->u.end.total_turns = (uint16_t)a;
        out->u.end.reason = (proto_end_reason_t)r;
        return ok ? PP_OK : PP_RANGE;
    }

    case PM_CFG:
        NEED(3);
        INT(1, 2, 6, a);
        INT(2, 10, 300, b);
        if (!valid_limit(b)) ok = false;
        out->u.cfg.players = (uint8_t)a;
        out->u.cfg.limit_s = (uint16_t)b;
        return ok ? PP_OK : PP_RANGE;

    case PM_CMD: {
        int cm;
        NEED(2);
        cm = lookup(f[1], CMD_NAMES, COUNT(CMD_NAMES));
        if (cm < 0) return PP_RANGE;
        out->u.cmd.cmd = (proto_cmd_t)cm;
        return PP_OK;
    }

    case PM_WIFI:
        NEED(2);
        INT(1, 0, 1, a);
        out->u.wifi.up = (a == 1);
        return ok ? PP_OK : PP_RANGE;

    case PM_ACK:
        if (nf != 2 && nf != 3) return PP_PARSE;
        out->u.ack.of = type_from_name(f[1]);
        if (out->u.ack.of == PM_INVALID) return PP_PARSE;
        if (nf == 3) {
            if (out->u.ack.of != PM_TURN) return PP_PARSE;
            INT(2, 1, 999, a);
            out->u.ack.has_turn = true;
            out->u.ack.turn_no = (uint16_t)a;
        } else if (out->u.ack.of == PM_TURN) {
            return PP_PARSE;                   /* ACK,TURN needs the turn number */
        }
        return ok ? PP_OK : PP_RANGE;

    case PM_ERR: {
        int r;
        NEED(3);
        out->u.err.of = type_from_name(f[1]);  /* may be PM_INVALID for ERR,<garbage>,UNKNOWN */
        r = lookup(f[2], ERR_NAMES, COUNT(ERR_NAMES));
        if (r < 0) return PP_PARSE;
        out->u.err.reason = (proto_err_t)r;
        return PP_OK;
    }

    default:
        return PP_UNKNOWN;
    }
#undef NEED
#undef INT
}

/* ---------------------------------------------------------------- format */

static void fmt_gid(char *dst, bool has, uint32_t id)
{
    if (has) snprintf(dst, 9, "%08lx", (unsigned long)id);
    else     strcpy(dst, "-");
}

int proto_format(const proto_msg_t *m, char *buf, size_t buflen)
{
    char gid[9];
    int n;

    switch (m->type) {
    case PM_HELLO:
        n = snprintf(buf, buflen, "HELLO,%s,%s\n", m->u.hello.role, m->u.hello.fw);
        break;
    case PM_STATE:
        fmt_gid(gid, m->u.state.has_game, m->u.state.game_id);
        n = snprintf(buf, buflen, "STATE,%s,%s,%u,%u,%u,%u,%ld\n", gid,
                     proto_state_name(m->u.state.state),
                     (unsigned)m->u.state.players, (unsigned)m->u.state.limit_s,
                     (unsigned)m->u.state.turn_no, (unsigned)m->u.state.player,
                     (long)m->u.state.remaining_ms);
        break;
    case PM_START:
        fmt_gid(gid, true, m->u.start.game_id);
        n = snprintf(buf, buflen, "START,%s,%u,%u\n", gid,
                     (unsigned)m->u.start.players, (unsigned)m->u.start.limit_s);
        break;
    case PM_TURN:
        fmt_gid(gid, true, m->u.turn.game_id);
        n = snprintf(buf, buflen, "TURN,%s,%u,%u,%lu,%lu\n", gid,
                     (unsigned)m->u.turn.turn_no, (unsigned)m->u.turn.player,
                     (unsigned long)m->u.turn.duration_ms, (unsigned long)m->u.turn.overtime_ms);
        break;
    case PM_END:
        fmt_gid(gid, true, m->u.end.game_id);
        n = snprintf(buf, buflen, "END,%s,%u,%s\n", gid, (unsigned)m->u.end.total_turns,
                     END_NAMES[m->u.end.reason == PR_RESET ? 1 : 0]);
        break;
    case PM_CFG:
        n = snprintf(buf, buflen, "CFG,%u,%u\n", (unsigned)m->u.cfg.players, (unsigned)m->u.cfg.limit_s);
        break;
    case PM_CMD:
        n = snprintf(buf, buflen, "CMD,%s\n", proto_cmd_name(m->u.cmd.cmd));
        break;
    case PM_WIFI:
        n = snprintf(buf, buflen, "WIFI,%d\n", m->u.wifi.up ? 1 : 0);
        break;
    case PM_ACK:
        if (m->u.ack.has_turn)
            n = snprintf(buf, buflen, "ACK,%s,%u\n", proto_type_name(m->u.ack.of), (unsigned)m->u.ack.turn_no);
        else
            n = snprintf(buf, buflen, "ACK,%s\n", proto_type_name(m->u.ack.of));
        break;
    case PM_ERR:
        n = snprintf(buf, buflen, "ERR,%s,%s\n", proto_type_name(m->u.err.of),
                     ((size_t)m->u.err.reason < COUNT(ERR_NAMES)) ? ERR_NAMES[m->u.err.reason] : "PARSE");
        break;
    default:
        return -1;
    }
    if (n < 0 || (size_t)n >= buflen || n > PROTO_MAX_LINE) return -1;
    return n;
}

/* ---------------------------------------------------------------- line assembler */

void proto_rx_init(proto_rx_t *rx)
{
    rx->len = 0;
    rx->overflow = false;
    rx->ready = false;
    rx->buf[0] = '\0';
}

bool proto_rx_push(proto_rx_t *rx, char c)
{
    if (rx->ready) {            /* previous line was consumed by the caller */
        rx->ready = false;
        rx->len = 0;
    }
    if (c == '\r') return false;
    if (c == '\n') {
        if (rx->overflow) {     /* end of a discarded over-long line */
            rx->overflow = false;
            rx->len = 0;
            return false;
        }
        rx->buf[rx->len] = '\0';
        rx->ready = true;
        return true;
    }
    if (rx->overflow) return false;
    if (rx->len >= PROTO_MAX_LINE - 1) {   /* no room left for content + '\n' */
        rx->overflow = true;
        rx->len = 0;
        return false;
    }
    rx->buf[rx->len++] = c;
    return false;
}
