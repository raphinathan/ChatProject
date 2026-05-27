#ifndef CHAT_CLIENT_NET_H
#define CHAT_CLIENT_NET_H

#include <stddef.h>
#include <stdint.h>

/* =========================================================================
 *  Client TCP transport - blocking, synchronous (one request -> one reply).
 *  No select() yet; the auth flow is strictly request/response.
 * ========================================================================= */

/* Connect to ip:port. Returns a socket fd >= 0, or -1 on failure. */
int  client_net_connect(const char* ip, uint16_t port);

/* Send exactly len bytes, looping over partial sends. 0 on success, -1 on error. */
int  client_net_send_all(int fd, const uint8_t* buf, size_t len);

/* Receive exactly one TLV frame into buf (must hold CHAT_MAX_MSG_SIZE).
 * Reads the 2-byte header, then L payload bytes; sets *out_len = 2 + L.
 * Returns 0 on success, -1 on disconnect / error / malformed header. */
int  client_net_recv_msg(int fd, uint8_t* buf, size_t* out_len);

void client_net_close(int fd);

#endif /* CHAT_CLIENT_NET_H */
