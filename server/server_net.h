#ifndef SERVER_NET_H
#define SERVER_NET_H

#include <stddef.h>
#include <stdint.h>
#include <signal.h>

typedef int  (*OnMessageFn)   (int _sockfd, const uint8_t* _msg, size_t _len, void* _ctx);
typedef void (*OnDisconnectFn)(int _sockfd, void* _ctx);

/* Opaque handle — same pattern as UserMng and GroupMng */
typedef struct ServerNet ServerNet;

/* Allocates state, creates and binds the listening socket.
 * Returns NULL on any failure. */
ServerNet* ServerNet_Create(uint16_t _port, OnMessageFn _onMsg,
                            OnDisconnectFn _onDisc, void* _ctx);

/* Blocks in the select() loop until *_keepRunning becomes 0.
 * Returns 0 on clean exit, -1 on fatal error. */
int ServerNet_Run(ServerNet* _net, const volatile sig_atomic_t* _keepRunning);

/* Sends _len bytes from _buf to socket _sockfd.
 * Returns bytes sent, or -1 on error. */
int ServerNet_SendMessage(int _sockfd, const uint8_t* _buf, size_t _len);

/* Closes all client connections, closes the listen socket, frees the struct. */
void ServerNet_Destroy(ServerNet** _net);

#endif /* SERVER_NET_H */
