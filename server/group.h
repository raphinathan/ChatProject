#ifndef GROUP_H
#define GROUP_H

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint16_t */
#include "../shared/protocol.h"

typedef struct 
{
    char name[CHAT_MAX_GROUPNAME_LEN + 1];
    char mcast_ip[CHAT_MAX_IP_STR_LEN];
    uint16_t mcast_port;
    
    /* When this reaches 0, the group is destroyed and the IP is reclaimed. */
    size_t member_count; 
    
} Group;

#endif /* GROUP_H */