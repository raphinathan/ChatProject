/* server/server_mng.c (Updated Phase 2) */

#include "server_mng.h"
#include "user_mng.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <sys/socket.h>

/* Global singleton for the Management layer state */
static UserMng* g_userMng = NULL;

/* --- Helper Function Declarations --- */

static void HandleRegisterReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleLoginReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleCreateGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleJoinGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleLeaveGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);

/* --- Main Functions --- */

int ServerMng_Init(void)
{
    g_userMng = UserMng_Create();
    
    if (NULL == g_userMng)
    {
        return 1; /* Init failed */
    }
    
    printf("ServerMng and UserMng Initialized.\n");
    return 0;
}

void ServerMng_Destroy(void)
{
    if (NULL != g_userMng)
    {
        UserMng_Destroy(&g_userMng);
    }
    printf("ServerMng Destroyed.\n");
}

int ServerMng_HandleMessage(int _sockfd, const uint8_t* _msg, size_t _len, void* _ctx)
{
    uint8_t type = 0;
    uint8_t length = 0;

    (void)_ctx;

    if (-1 == chat_peek_header(_msg, _len, &type, &length))
    {
        return -1;
    }
    printf("Received Opcode 0x%02X from FD %d\n", type, _sockfd);

    switch ((ChatOpcode)type)
    {
        case OP_REG_REQ:
            HandleRegisterReq(_sockfd, _msg, _len);
            break;
            
        case OP_LOGIN_REQ:
            HandleLoginReq(_sockfd, _msg, _len);
            break;
            
        case OP_LOGOUT_REQ:
            {
                uint8_t repBuf[CHAT_MAX_MSG_SIZE];
                ChatStatus status = UserMng_Logout(g_userMng, _sockfd);
                int repLen = chat_encode_status_rep(repBuf, OP_LOGOUT_REP, status);
                
                printf("  -> Logout requested for FD: %d\n", _sockfd);
                if (repLen > 0) 
                {
                    send(_sockfd, repBuf, (size_t)repLen, 0);
                }
            }
            break;
            
        case OP_CREATE_GROUP_REQ:
            HandleCreateGroupReq(_sockfd, _msg, _len);
            break;
            
        case OP_JOIN_GROUP_REQ:
            HandleJoinGroupReq(_sockfd, _msg, _len);
            break;
            
        case OP_LEAVE_GROUP_REQ:
            HandleLeaveGroupReq(_sockfd, _msg, _len);
            break;
            
        default:
            break;
    }

    return 0;
}

void ServerMng_OnDisconnect(int _sockfd, void* _ctx)
{
    (void)_ctx;
    
    if (NULL != g_userMng)
    {
        UserMng_Disconnect(g_userMng, _sockfd);
    }
    
    printf("ServerMng handled disconnect for FD %d.\n", _sockfd);
}

/* --- Helper Function Definitions --- */

static void HandleRegisterReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char user[CHAT_MAX_USERNAME_LEN + 1] = {0};
    char pass[CHAT_MAX_PASSWORD_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;
    ChatStatus status;

    if (0 == chat_decode_user_pass(_msg, _len, user, pass))
    {
        printf("  -> Register requested for User: %s\n", user);
        status = UserMng_Register(g_userMng, user, pass);
        
        repLen = (size_t)chat_encode_status_rep(repBuf, OP_REG_REP, status);
        send(_sockfd, repBuf, repLen, 0);
    }
}

static void HandleLoginReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char user[CHAT_MAX_USERNAME_LEN + 1] = {0};
    char pass[CHAT_MAX_PASSWORD_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;
    ChatStatus status;

    if (0 == chat_decode_user_pass(_msg, _len, user, pass))
    {
        printf("  -> Login requested for User: %s\n", user);
        status = UserMng_Login(g_userMng, user, pass, _sockfd);
        
        repLen = (size_t)chat_encode_status_rep(repBuf, OP_LOGIN_REP, status);
        send(_sockfd, repBuf, repLen, 0);
    }
}

/* * Phase 1 Stubs remain intact for Group features until we build GroupMng 
 */

static void HandleCreateGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char group[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    chat_decode_groupname(_msg, _len, group);
    repLen = (size_t)chat_encode_group_rep_ok(repBuf, OP_CREATE_GROUP_REP, "239.1.1.99", 6000);
    send(_sockfd, repBuf, repLen, 0);
}

static void HandleJoinGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char group[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    chat_decode_groupname(_msg, _len, group);
    repLen = (size_t)chat_encode_group_rep_ok(repBuf, OP_JOIN_GROUP_REP, "239.1.1.99", 6000);
    send(_sockfd, repBuf, repLen, 0);
}

static void HandleLeaveGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char group[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    chat_decode_groupname(_msg, _len, group);
    repLen = (size_t)chat_encode_status_rep(repBuf, OP_LEAVE_GROUP_REP, ST_OK);
    send(_sockfd, repBuf, repLen, 0);
}