#include "protocol.h"

#include <string.h>

/* =========================================================================
 * turns a request into bytes / bytes into a result
 * TLV codec - auth subset (REG/LOGIN req + status rep).
 *
 *  Frame:   [T:1][L:1][payload:L]
 *  Sub-TLV: [Li:1][Vi:Li]   (every field, fixed or variable, is wrapped)
 *
 *  Encoders write into a caller buffer of CHAT_MAX_MSG_SIZE and return the
 *  total bytes written (T + L + payload), or -1 on bad arg / overflow.
 *  Decoders read a buffer of known length, fill outputs, return 0 / -1.
 *  Decoders never read past buf_len: a length byte that would overrun the
 *  buffer is treated as malformed input, not trusted.
 * ========================================================================= */

/* ------------------------------------------------------------------------- */
int chat_peek_header(const uint8_t* buf, size_t buf_len,
                     uint8_t* out_type, uint8_t* out_len)
{
    if (!buf || !out_type || !out_len || buf_len < 2) {
        return -1;
    }
    *out_type = buf[0];
    *out_len  = buf[1];
    return 0;
}

/* ------------------------------------------------------------------------- */
int chat_encode_user_pass_req(uint8_t* buf, ChatOpcode op,
                              const char* user, const char* pass)
{
    size_t ulen, plen, payload;

    if (!buf || !user || !pass) {
        return -1;
    }
    ulen = strlen(user);
    plen = strlen(pass);
    if (ulen > CHAT_MAX_USERNAME_LEN || plen > CHAT_MAX_PASSWORD_LEN) {
        return -1;
    }

    /* payload = [Lu][user][Lp][pass] */
    payload = 1 + ulen + 1 + plen;
    if (payload > CHAT_MAX_PAYLOAD) {
        return -1;
    }

    buf[0] = (uint8_t)op;
    buf[1] = (uint8_t)payload;
    buf[2] = (uint8_t)ulen;
    memcpy(&buf[3], user, ulen);
    buf[3 + ulen] = (uint8_t)plen;
    memcpy(&buf[4 + ulen], pass, plen);

    return (int)(2 + payload);
}

/* ------------------------------------------------------------------------- */
int chat_decode_user_pass(const uint8_t* buf, size_t buf_len,
                          char* out_user, char* out_pass)
{
    size_t pos, payload, ulen, plen;

    if (!buf || !out_user || !out_pass || buf_len < 2) {
        return -1;
    }

    payload = buf[1];
    if (2 + payload > buf_len) {
        return -1;                 /* frame claims more than we received */
    }

    /* [Lu][user] */
    pos = 2;
    if (pos >= buf_len) {
        return -1;
    }
    ulen = buf[pos++];
    if (ulen > CHAT_MAX_USERNAME_LEN || pos + ulen > buf_len) {
        return -1;
    }
    memcpy(out_user, &buf[pos], ulen);
    out_user[ulen] = '\0';
    pos += ulen;

    /* [Lp][pass] */
    if (pos >= buf_len) {
        return -1;
    }
    plen = buf[pos++];
    if (plen > CHAT_MAX_PASSWORD_LEN || pos + plen > buf_len) {
        return -1;
    }
    memcpy(out_pass, &buf[pos], plen);
    out_pass[plen] = '\0';

    return 0;
}

/* ------------------------------------------------------------------------- */
int chat_encode_status_rep(uint8_t* buf, ChatOpcode op, ChatStatus status)
{
    if (!buf) {
        return -1;
    }
    buf[0] = (uint8_t)op;
    buf[1] = 1;                    /* payload length: [L=1][code] */
    buf[2] = (uint8_t)status;
    return 3;
}

/* ------------------------------------------------------------------------- */
int chat_decode_status_rep(const uint8_t* buf, size_t buf_len,
                           ChatStatus* out_status)
{
    if (!buf || !out_status || buf_len < 3) {
        return -1;
    }
    if (buf[1] != 1) {             /* status payload must be [L=1][code] */
        return -1;
    }
    *out_status = (ChatStatus)buf[2];
    return 0;
}

/* ------------------------------------------------------------------------- */
int chat_encode_groupname_req(uint8_t* buf, ChatOpcode op, const char* group)
{
    /* Cast unused parameters to void to satisfy -Wall and -Wextra */
    (void)group;

    if (NULL != buf)
    {
        buf[0] = (uint8_t)op;
        buf[1] = 0; /* Dummy length of 0 */
    }
    
    return 2; /* Returns T(1) + L(1) = 2 bytes */
}

/* ------------------------------------------------------------------------- */
int chat_decode_groupname(const uint8_t* buf, size_t buf_len, char* out_group)
{
    (void)buf;
    (void)buf_len;

    /* Safely populate the output buffer with a dummy string so the 
     * server's printf statements have something valid to read. */
    if (NULL != out_group)
    {
        strcpy(out_group, "dummy_grp");
    }
    
    return 0; /* 0 indicates success in the protocol design */
}

/* ------------------------------------------------------------------------- */
int chat_encode_logout_req(uint8_t* buf)
{
    if (NULL != buf)
    {
        buf[0] = (uint8_t)OP_LOGOUT_REQ;
        buf[1] = 0;
    }
    
    return 2;
}

/* ------------------------------------------------------------------------- */
int chat_encode_group_rep_ok(uint8_t* buf, ChatOpcode op, const char* mcast_ip, uint16_t mcast_port)
{
    (void)mcast_ip;
    (void)mcast_port;

    if (NULL != buf)
    {
        buf[0] = (uint8_t)op;
        buf[1] = 0; 
    }
    
    return 2;
}

/* ------------------------------------------------------------------------- */
int chat_decode_group_rep_ok(const uint8_t* buf, size_t buf_len, ChatStatus* out_status, char* out_mcast_ip, uint16_t* out_mcast_port)
{
    (void)buf;
    (void)buf_len;

    /* Provide safe dummy values to the client if they attempt to decode this */
    if (NULL != out_status) 
    {
        *out_status = ST_OK;
    }
    if (NULL != out_mcast_ip) 
    {
        strcpy(out_mcast_ip, "239.1.1.99");
    }
    if (NULL != out_mcast_port) 
    {
        *out_mcast_port = 6000;
    }
    
    return 0;
}