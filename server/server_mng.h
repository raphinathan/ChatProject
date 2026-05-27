#ifndef SERVER_MNG_H
#define SERVER_MNG_H

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint8_t */

int ServerMng_Init(void);
void ServerMng_Destroy(void);
int ServerMng_HandleMessage(int _sockfd, const uint8_t* _msg, size_t _len, void* _ctx);
void ServerMng_OnDisconnect(int _sockfd, void* _ctx);

#endif /* SERVER_MNG_H */