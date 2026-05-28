#include "free_mc_queue.h"
#include "../shared/adt/genqueue.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct FreeMcQueue 
{
    Queue* pool;
};

/* --- Helper Function Declarations --- */
static void DestroyPairCallback(void* _item);

/* --- Main Functions --- */

FreeMcQueue* FreeMcQueue_Create(void)
{
    FreeMcQueue* fmq = NULL;
    McastPair* pair = NULL;
    int i = 0;

    fmq = (FreeMcQueue*)malloc(sizeof(FreeMcQueue));
    if (NULL == fmq)
    {
        return NULL;
    }

    /* Allocate capacity for 255 IPs */
    fmq->pool = QueueCreate(255);
    if (NULL == fmq->pool)
    {
        free(fmq);
        return NULL;
    }

    /* Pre-fill the queue with IPs 239.1.1.1 through 239.1.1.255 */
    for (i = 1; i <= 255; ++i)
    {
        pair = (McastPair*)malloc(sizeof(McastPair));
        if (NULL != pair)
        {
            sprintf(pair->ip, "239.1.1.%d", i);
            pair->port = CHAT_MCAST_BASE_PORT;
            if (QUEUE_SUCCESS != QueueInsert(fmq->pool, pair))
            {
                free(pair);
            }

        }
    }

    return fmq;
}

void FreeMcQueue_Destroy(FreeMcQueue** _queue)
{
    if (NULL == _queue || NULL == *_queue)
    {
        return;
    }

    QueueDestroy(&((*_queue)->pool), DestroyPairCallback);
    
    free(*_queue);
    *_queue = NULL;
}

int FreeMcQueue_Take(FreeMcQueue* _queue, McastPair* _outPair)
{
    McastPair* poppedPair = NULL;

    if (NULL == _queue || NULL == _outPair)
    {
        return 0;
    }

    /* If Dequeue fails, the IP pool is exhausted */
    if (QUEUE_SUCCESS != QueueRemove(_queue->pool, (void**)&poppedPair) || NULL == poppedPair)
    {
        return 0; 
    }

    /* Safely copy the data out and free the node we just popped */
    strcpy(_outPair->ip, poppedPair->ip);
    _outPair->port = poppedPair->port;
    
    free(poppedPair);
    return 1;
}

int FreeMcQueue_Return(FreeMcQueue* _queue, const McastPair* _pair)
{
    McastPair* newPair = NULL;

    if (NULL == _queue || NULL == _pair)
    {
        return 0;
    }

    newPair = (McastPair*)malloc(sizeof(McastPair));
    if (NULL == newPair)
    {
        return 0;
    }

    strcpy(newPair->ip, _pair->ip);
    newPair->port = _pair->port;

    if (QUEUE_SUCCESS != QueueInsert(_queue->pool, newPair))
    {
        free(newPair);
        return 0;
    }

    return 1;
}

/* --- Helper Function Definitions --- */

static void DestroyPairCallback(void* _item)
{
    if (NULL != _item)
    {
        free(_item);
    }
}