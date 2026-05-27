#include "server_mng.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <sys/socket.h>

/* --- Helper Function Declarations --- */

static void HandleRegisterReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleLoginReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleCreateGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleJoinGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleLeaveGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);

/* --- Main Functions --- */

int ServerMng_Init(void)
{
    printf("ServerMng Initialized.\n");
    return 1;
}

void ServerMng_Destroy(void)
{
    printf("ServerMng Destroyed.\n");
}

int ServerMng_HandleMessage(int _sockfd, const uint8_t* _msg, size_t _len, void* _ctx)
{
    uint8_t type = 0;
    uint8_t length = 0;

    /* Casting a parameter to (void) is the standard C89 trick to tell the compiler: 
     * "I know I am not using this variable yet, do not throw a warning." */
    (void)_ctx;

    /* We safely peek again. We know it's valid because server_net checked it, 
     * but we need to extract the 'type' to know how to route it. */
    if (-1 == chat_peek_header(_msg, _len, &type, &length))
    {
        return -1;
    }

    /* Cast 'type' to ChatOpcode to utilize the compiler's type checking. */
    switch ((ChatOpcode)type)
    {
        case OP_REG_REQ:
            HandleRegisterReq(_sockfd, _msg, _len);
            break;
            
        case OP_LOGIN_REQ:
            HandleLoginReq(_sockfd, _msg, _len);
            break;
            
        case OP_LOGOUT_REQ:
            /* In strict C89, variables must be declared at the TOP of a block. 
             * By wrapping this case in braces {}, we create a new scope block, 
             * allowing us to declare repBuf and repLen locally without polluting 
             * the rest of the switch statement. */
            {
                uint8_t repBuf[CHAT_MAX_MSG_SIZE];
                size_t repLen = chat_encode_status_rep(repBuf, OP_LOGOUT_REP, ST_OK);
                send(_sockfd, repBuf, repLen, 0);
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
    printf("ServerMng handling disconnect for FD %d.\n", _sockfd);
}

/* --- Helper Function Definitions --- */

static void HandleRegisterReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char user[CHAT_MAX_USERNAME_LEN + 1] = {0};
    char pass[CHAT_MAX_PASSWORD_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    if (0 == chat_decode_user_pass(_msg, _len, user, pass))
    {
        printf("  -> Register requested for User: %s\n", user);
    }

    repLen = chat_encode_status_rep(repBuf, OP_REG_REP, ST_OK);
    send(_sockfd, repBuf, repLen, 0);
}

static void HandleLoginReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char user[CHAT_MAX_USERNAME_LEN + 1] = {0};
    char pass[CHAT_MAX_PASSWORD_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    if (0 == chat_decode_user_pass(_msg, _len, user, pass))
    {
        printf("  -> Login requested for User: %s\n", user);
    }

    repLen = chat_encode_status_rep(repBuf, OP_LOGIN_REP, ST_OK);
    send(_sockfd, repBuf, repLen, 0);
}

static void HandleCreateGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char group[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    if (0 == chat_decode_groupname(_msg, _len, group))
    {
        printf("  -> Create Group requested: %s\n", group);
    }

    repLen = chat_encode_group_rep_ok(repBuf, OP_CREATE_GROUP_REP, "239.1.1.99", 6000);
    send(_sockfd, repBuf, repLen, 0);
}

static void HandleJoinGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char group[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    if (0 == chat_decode_groupname(_msg, _len, group))
    {
        printf("  -> Join Group requested: %s\n", group);
    }

    repLen = chat_encode_group_rep_ok(repBuf, OP_JOIN_GROUP_REP, "239.1.1.99", 6000);
    send(_sockfd, repBuf, repLen, 0);
}

static void HandleLeaveGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char group[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    size_t repLen = 0;

    if (0 == chat_decode_groupname(_msg, _len, group))
    {
        printf("  -> Leave Group requested: %s\n", group);
    }

    repLen = chat_encode_status_rep(repBuf, OP_LEAVE_GROUP_REP, ST_OK);
    send(_sockfd, repBuf, repLen, 0);
}