#ifndef CHAT_PROTOCOL_H
#define CHAT_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* =========================================================================
 *  Chat Project - TCP Control Protocol
 *  Wire format: TLV (Type-Length-Value), 1-byte Type, 1-byte Length.
 *
 *      +-------+-------+--------------------+
 *      |   T   |   L   |   payload (L B)    |
 *      +-------+-------+--------------------+
 *
 *  Variable-length sub-fields inside the payload use the same shape:
 *      [L_i : 1 byte][V_i : L_i bytes]   (e.g. username, password, group name)
 *  Fixed-size sub-fields are still wrapped in [L][V] for uniform parsing
 *  (e.g. port = [L=2][hi][lo], status = [L=1][code]).
 *
 *  Example - REG_REQ for username "abc" / password "123":
 *      01 08  03 'a''b''c'  03 '1''2''3'
 *       T  L   L1  V1.....   L2  V2.....
 *
 *  All multi-byte integers on the wire are big-endian (network order).
 * ========================================================================= */

#define CHAT_TCP_PORT          5000
#define CHAT_MAX_PAYLOAD       255      /* L is 1 byte => max 255 payload   */
#define CHAT_MAX_MSG_SIZE      (2 + CHAT_MAX_PAYLOAD)  /* T + L + payload   */

#define CHAT_MAX_USERNAME_LEN  32
#define CHAT_MAX_PASSWORD_LEN  32
#define CHAT_MAX_GROUPNAME_LEN 32
#define CHAT_MAX_IP_STR_LEN    16       /* "255.255.255.255" + NUL          */

/* Multicast pool for groups (admin-scoped 239.0.0.0/8). */
#define CHAT_MCAST_BASE_IP     "239.1.1.1"
#define CHAT_MCAST_BASE_PORT   6000

/* ---------------------------- Opcodes (Type) ---------------------------- */
typedef enum {
    OP_REG_REQ          = 0x01,
    OP_REG_REP          = 0x02,

    OP_LOGIN_REQ        = 0x03,
    OP_LOGIN_REP        = 0x04,

    OP_LOGOUT_REQ       = 0x05,
    OP_LOGOUT_REP       = 0x06,

    OP_CREATE_GROUP_REQ = 0x07,
    OP_CREATE_GROUP_REP = 0x08,

    OP_JOIN_GROUP_REQ   = 0x09,
    OP_JOIN_GROUP_REP   = 0x0A,

    OP_LEAVE_GROUP_REQ  = 0x0B,
    OP_LEAVE_GROUP_REP  = 0x0C
} ChatOpcode;

/* ----------------------------- Status codes ----------------------------- */
typedef enum {
    ST_OK                    = 0x00,
    ST_ERR_USER_EXISTS       = 0x01,
    ST_ERR_USER_NOT_FOUND    = 0x02,
    ST_ERR_BAD_PASSWORD      = 0x03,
    ST_ERR_ALREADY_LOGGED_IN = 0x04,
    ST_ERR_NOT_LOGGED_IN     = 0x05,
    ST_ERR_GROUP_EXISTS      = 0x06,
    ST_ERR_GROUP_NOT_FOUND   = 0x07,
    ST_ERR_ALREADY_IN_GROUP  = 0x08,
    ST_ERR_NOT_IN_GROUP      = 0x09,
    ST_ERR_SERVER_FULL       = 0x0A,
    ST_ERR_PROTOCOL          = 0xFF
} ChatStatus;

/* =========================================================================
 *  Payload shapes (informal, but the encoder/decoder enforce them).
 *
 *  REG_REQ   / LOGIN_REQ : [L][username] [L][password]
 *  REG_REP   / LOGIN_REP / LOGOUT_REP / CREATE_GROUP_REP (failure) /
 *              LEAVE_GROUP_REP                      : [L=1][status]
 *
 *  LOGOUT_REQ                                       : (empty payload, L=0)
 *
 *  CREATE_GROUP_REQ / JOIN_GROUP_REQ /
 *  LEAVE_GROUP_REQ                                  : [L][groupname]
 *
 *  JOIN_GROUP_REP (success) / CREATE_GROUP_REP (success):
 *      [L=1][status=OK] [L][mcast_ip_str] [L=2][port_hi][port_lo]
 * ========================================================================= */

/* -------------------- Encode / Decode helper API ------------------------ *
 *  All encoders write into a caller-provided buffer of CHAT_MAX_MSG_SIZE
 *  and return the total number of bytes written (T + L + payload), or
 *  -1 on overflow / bad argument.
 *
 *  All decoders read from a buffer of known length and return 0 on success
 *  or -1 on malformed input. Output pointers must be non-NULL.
 * ------------------------------------------------------------------------ */

/* Generic header peek: get T and L without consuming the payload.
 * Returns 0 on success, -1 if buf_len < 2.                                 */
int chat_peek_header(const uint8_t* buf, size_t buf_len,
                     uint8_t* out_type, uint8_t* out_len);

/* Build a one-byte-status reply (REG_REP, LOGIN_REP, LOGOUT_REP, ...).    */
int chat_encode_status_rep(uint8_t* buf, ChatOpcode op, ChatStatus status);

/* Build a [user][pass] request (REG_REQ, LOGIN_REQ).                      */
int chat_encode_user_pass_req(uint8_t* buf, ChatOpcode op,
                              const char* user, const char* pass);

/* Build a single-string request (CREATE/JOIN/LEAVE group).                */
int chat_encode_groupname_req(uint8_t* buf, ChatOpcode op, const char* group);

/* Build LOGOUT_REQ (empty payload).                                       */
int chat_encode_logout_req(uint8_t* buf);

/* Build JOIN_GROUP_REP / CREATE_GROUP_REP success payload.                */
int chat_encode_group_rep_ok(uint8_t* buf, ChatOpcode op,
                             const char* mcast_ip, uint16_t mcast_port);

/* Decoders. user/group/pass buffers must be at least CHAT_MAX_*_LEN+1.    */
int chat_decode_user_pass(const uint8_t* buf, size_t buf_len,
                          char* out_user, char* out_pass);

int chat_decode_groupname(const uint8_t* buf, size_t buf_len,
                          char* out_group);

int chat_decode_status_rep(const uint8_t* buf, size_t buf_len,
                           ChatStatus* out_status);

int chat_decode_group_rep_ok(const uint8_t* buf, size_t buf_len,
                             ChatStatus* out_status,
                             char* out_mcast_ip, uint16_t* out_mcast_port);

#endif /* CHAT_PROTOCOL_H */

