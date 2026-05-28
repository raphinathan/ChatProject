#ifndef USER_H
#define USER_H

#include "../shared/protocol.h"
#include "../shared/adt/gen_dlist.h"

/* * We use an enum for state. This prevents bugs where someone 
 * accidentally sets the state to 99 instead of 1. 
 */
typedef enum 
{
    USER_OFFLINE = 0,
    USER_ONLINE  = 1
} UserState;

typedef struct 
{
    char username[CHAT_MAX_USERNAME_LEN + 1]; /* +1 for the null terminator */
    char password[CHAT_MAX_PASSWORD_LEN + 1];
    
    UserState state;
    
    /* * The file descriptor linking this application layer struct 
     * to the transport layer connection. It will be -1 when offline. 
     */
    int socketFd; 
    
    /* * A doubly linked list storing the names of groups this user is in. 
     * We will use this in Phase 3 to achieve O(k) group removal on logout. 
     */
    List* joinedGroups; 
    
} User;

#endif /* USER_H */