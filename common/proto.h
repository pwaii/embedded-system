/*
 * proto.h — STM32 <-> ESP32 line protocol v1.0 (see docs/protocol.md)
 *
 * Shared by both firmwares. Pure C99, no malloc, no globals, no hardware.
 *   STM32:  add common/ to the CubeIDE include path and compile proto.c.
 *   ESP32:  copy (or symlink) proto.h/proto.c into the sketch/src folder; it compiles as C.
 *   PC:     make -C common test
 */
#ifndef PROTO_H
#define PROTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_VERSION    "1.0"
#define PROTO_MAX_LINE   64   /* bytes including the trailing '\n' */

typedef enum {
    PM_INVALID = 0,
    /* STM32 -> ESP32 */
    PM_HELLO, PM_STATE, PM_START, PM_TURN, PM_END,
    /* ESP32 -> STM32 */
    PM_CFG, PM_CMD, PM_WIFI,
    /* both directions */
    PM_ACK, PM_ERR
} proto_type_t;

typedef enum { PS_SETUP = 0, PS_READY, PS_RUN, PS_PAUSE, PS_OVER } proto_state_t;
typedef enum { PC_START = 0, PC_PAUSE, PC_RESUME, PC_END, PC_RESET } proto_cmd_t;
typedef enum { PE_PARSE = 0, PE_RANGE, PE_BUSY, PE_UNKNOWN } proto_err_t;
typedef enum { PR_USER = 0, PR_RESET } proto_end_reason_t;

typedef struct {
    proto_type_t type;
    union {
        struct { char role[4]; char fw[12]; } hello;               /* HELLO,STM,1.0 */
        struct {
            bool          has_game;     /* false => game_id field is "-" */
            uint32_t      game_id;
            proto_state_t state;
            uint8_t       players;
            uint16_t      limit_s;
            uint16_t      turn_no;
            uint8_t       player;
            int32_t       remaining_ms; /* negative = overtime */
        } state;
        struct { uint32_t game_id; uint8_t players; uint16_t limit_s; } start;
        struct {
            uint32_t game_id;
            uint16_t turn_no;
            uint8_t  player;
            uint32_t duration_ms;
            uint32_t overtime_ms;
        } turn;
        struct { uint32_t game_id; uint16_t total_turns; proto_end_reason_t reason; } end;
        struct { uint8_t players; uint16_t limit_s; } cfg;
        struct { proto_cmd_t cmd; } cmd;
        struct { bool up; } wifi;
        struct { proto_type_t of; bool has_turn; uint16_t turn_no; } ack;   /* ACK,CFG / ACK,TURN,7 */
        struct { proto_type_t of; proto_err_t reason; } err;                /* ERR,CFG,BUSY */
    } u;
} proto_msg_t;

typedef enum {
    PP_OK = 0,
    PP_EMPTY,      /* blank line: ignore silently */
    PP_TOO_LONG,   /* > PROTO_MAX_LINE: ignore */
    PP_UNKNOWN,    /* unknown message type: reply ERR,<type>,UNKNOWN */
    PP_PARSE,      /* wrong field count / not a number: reply ERR,<type>,PARSE */
    PP_RANGE       /* value outside docs/protocol.md ranges: reply ERR,<type>,RANGE */
} proto_result_t;

/* Parse one line (with or without trailing "\r\n").
 * On error, out->type still holds the recognised type (or PM_INVALID) so the caller can build ERR. */
proto_result_t proto_parse_line(const char *line, proto_msg_t *out);

/* Format a message including the trailing '\n'. Returns length written, or -1 if invalid/too small. */
int proto_format(const proto_msg_t *m, char *buf, size_t buflen);

/* Map a parse error to the ERR reason to send back (PP_UNKNOWN->PE_UNKNOWN, etc.). */
proto_err_t proto_err_from_result(proto_result_t r);

const char *proto_type_name(proto_type_t t);
const char *proto_state_name(proto_state_t s);
const char *proto_cmd_name(proto_cmd_t c);

/* ---- Line assembler: feed bytes from the UART, get whole lines out. ---- */
typedef struct {
    char   buf[PROTO_MAX_LINE + 1];
    size_t len;
    bool   overflow;   /* currently discarding an over-long line */
    bool   ready;      /* buf holds a complete line from the previous push */
} proto_rx_t;

void proto_rx_init(proto_rx_t *rx);
/* Returns true when a complete line is ready in rx->buf (NUL-terminated, no '\n').
 * Call proto_parse_line(rx->buf, ...) then keep feeding; the buffer is reset on the next byte. */
bool proto_rx_push(proto_rx_t *rx, char c);

#ifdef __cplusplus
}
#endif
#endif /* PROTO_H */
