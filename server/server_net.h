#ifndef SERVER_NET_H
#define SERVER_NET_H

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint8_t, uint16_t */
#include <signal.h> /* sig_atomic_t */

/* Callbacks for the management layer */
typedef int (*OnMessageFn)(int _sockfd, const uint8_t* _msg, size_t _len, void* _ctx);
typedef void (*OnDisconnectFn)(int _sockfd, void* _ctx);

/* * Starts the blocking select() loop. 
 * Returns 0 on normal exit, -1 on fatal error.
 */
int ServerNet_Run(uint16_t _port, OnMessageFn _onMsg, OnDisconnectFn _onDisc,
                  void* _ctx, const volatile sig_atomic_t* _keepRunning);

#endif /* SERVER_NET_H */