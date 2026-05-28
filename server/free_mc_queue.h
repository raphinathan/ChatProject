#ifndef FREE_MC_QUEUE_H
#define FREE_MC_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include "../shared/protocol.h"

typedef struct FreeMcQueue FreeMcQueue;

typedef struct 
{
    char ip[CHAT_MAX_IP_STR_LEN];
    uint16_t port;
} McastPair;

/* Creates the queue and pre-fills it with 255 Multicast IPs */
FreeMcQueue* FreeMcQueue_Create(void);

void FreeMcQueue_Destroy(FreeMcQueue** _queue);

/* Returns 1 on success, 0 if no IPs are left in the pool */
int FreeMcQueue_Take(FreeMcQueue* _queue, McastPair* _outPair);

/* Returns 1 on success */
int FreeMcQueue_Return(FreeMcQueue* _queue, const McastPair* _pair);

#endif /* FREE_MC_QUEUE_H */