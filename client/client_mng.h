#ifndef CHAT_CLIENT_MNG_H
#define CHAT_CLIENT_MNG_H

#include <stdint.h>

#include "protocol.h"

/* =========================================================================
 *  Client Mng - owns the TCP connection + client-side state. Each call is
 *  synchronous: send the TLV request, block for the reply, return the status.
 * ========================================================================= */

typedef struct ClientMng ClientMng;

/* Connect to server_ip:port. Returns NULL on connect failure. */
ClientMng* client_mng_create(const char* server_ip, uint16_t port);

/* Close socket and free. Safe on NULL / *pm == NULL. */
void       client_mng_destroy(ClientMng** pm);

/* Auth requests. Return a ChatStatus (ST_OK on success), or ST_ERR_PROTOCOL
 * on any transport / encoding failure. */
ChatStatus client_mng_register(ClientMng* m, const char* user, const char* pass);
ChatStatus client_mng_login   (ClientMng* m, const char* user, const char* pass);
ChatStatus client_mng_logout  (ClientMng* m);

/* Group requests. */
ChatStatus client_mng_create_group(ClientMng* m, const char* group,
                                   char* out_ip, uint16_t* out_port);
ChatStatus client_mng_join_group  (ClientMng* m, const char* group,
                                   char* out_ip, uint16_t* out_port);
ChatStatus client_mng_leave_group (ClientMng* m, const char* group);

/* True once a login has succeeded. */
int         client_mng_is_logged_in(const ClientMng* m);
const char* client_mng_username     (const ClientMng* m);

#endif /* CHAT_CLIENT_MNG_H */
