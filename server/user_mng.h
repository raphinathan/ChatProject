#ifndef USER_MNG_H
#define USER_MNG_H

#include "../shared/protocol.h"

/* Opaque pointer declaration. The actual struct is hidden in the .c file. */
typedef struct UserMng UserMng;

/* * Allocates the User Manager and initializes the underlying HashMaps.
 * Returns NULL on memory failure. 
 */
UserMng* UserMng_Create(void);

/* * Destroys the manager and frees all allocated Users inside it. 
 */
void UserMng_Destroy(UserMng** _mng);

/* * Phase 2 Core API 
 */
ChatStatus UserMng_Register(UserMng* _mng, const char* _username, const char* _password);

ChatStatus UserMng_Login(UserMng* _mng, const char* _username, const char* _password, int _sockfd);

/* Callback definition so UserMng can talk to GroupMng without a circular dependency */
typedef void (*LeaveGroupCallback)(const char* _groupName, void* _ctx);

/* Join/Leave tracking functions */
ChatStatus UserMng_JoinGroup(UserMng* _mng, int _sockfd, const char* _groupName);
ChatStatus UserMng_LeaveGroup(UserMng* _mng, int _sockfd, const char* _groupName);

/* Returns 1 if the socket belongs to a currently logged-in user, 0 otherwise. */
int UserMng_IsLoggedIn(const UserMng* _mng, int _sockfd);
/* * Handles sudden network drops (e.g., SIGPIPE or recv() returning 0). 
 * It implicitly logs the user out. 
 */
ChatStatus UserMng_Logout(UserMng* _mng, int _sockfd, LeaveGroupCallback _leaveCb, void* _ctx);
void UserMng_Disconnect(UserMng* _mng, int _sockfd, LeaveGroupCallback _leaveCb, void* _ctx);

#endif /* USER_MNG_H */