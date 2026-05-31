#include "server_mng.h"
#include "user_mng.h"
#include "free_mc_queue.h"
#include "group_mng.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <sys/socket.h>

/* Global singleton for the Management layer state */
static UserMng* g_userMng = NULL;
static GroupMng* g_groupMng = NULL;
static FreeMcQueue* g_mcQueue = NULL;

/* --- Helper Function Declarations --- */
static void HandleRegisterReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleLoginReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleCreateGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleJoinGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);
static void HandleLeaveGroupReq(int _sockfd, const uint8_t* _msg, size_t _len);

/* Wrapper to match the LeaveGroupCallback signature in UserMng */
static void OnUserImplicitLeave(const char* _groupName, void* _ctx)
{
    GroupMng* mng = (GroupMng*)_ctx;
    GroupMng_LeaveGroup(mng, _groupName);
}
/* --- Main Functions --- */

int ServerMng_Init(void)
{
    g_mcQueue = FreeMcQueue_Create();
    if (NULL == g_mcQueue)
    {
        return -1;
    }

    g_groupMng = GroupMng_Create(g_mcQueue);
    if (NULL == g_groupMng)
    {
        FreeMcQueue_Destroy(&g_mcQueue);
        return -1;
    }

    g_userMng = UserMng_Create();
    if (NULL == g_userMng)
    {
        GroupMng_Destroy(&g_groupMng);
        FreeMcQueue_Destroy(&g_mcQueue);
        return -1;
    }
    
    printf("Server Management Systems Fully Initialized.\n");
    return 0;
}

void ServerMng_Destroy(void)
{
    if (NULL != g_userMng) 
    {
        UserMng_Destroy(&g_userMng);
    }
    if (NULL != g_groupMng) 
    {
        GroupMng_Destroy(&g_groupMng);
    }
    if (NULL != g_mcQueue) 
    {
        FreeMcQueue_Destroy(&g_mcQueue);
    }
    
    printf("Server Management Systems Destroyed.\n");
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
                ChatStatus status;
                int repLen = 0;

                printf("  -> Logout requested for FD: %d\n", _sockfd);
                
                /* UserMng will auto-trigger OnUserImplicitLeave for every group they were in */
                status = UserMng_Logout(g_userMng, _sockfd, OnUserImplicitLeave, g_groupMng);
                
                repLen = chat_encode_status_rep(repBuf, OP_LOGOUT_REP, status);
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
        /* Triggers the exact same cleanup as a graceful logout */
        UserMng_Disconnect(g_userMng, _sockfd, OnUserImplicitLeave, g_groupMng);
    }
    
    printf("ServerMng handled disconnect for FD %d.\n", _sockfd);
}

/* --- Helper Function Definitions --- */

static void HandleRegisterReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char user[CHAT_MAX_USERNAME_LEN + 1] = {0};
    char pass[CHAT_MAX_PASSWORD_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    int repLen = 0;
    ChatStatus status;

    if (0 == chat_decode_user_pass(_msg, _len, user, pass))
    {
        printf("  -> Register requested for User: %s\n", user);
        status = UserMng_Register(g_userMng, user, pass);

        repLen = chat_encode_status_rep(repBuf, OP_REG_REP, status);
        if (repLen > 0) 
        { 
            send(_sockfd, repBuf, (size_t)repLen, 0); 
        }
    }
}

static void HandleLoginReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char user[CHAT_MAX_USERNAME_LEN + 1] = {0};
    char pass[CHAT_MAX_PASSWORD_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    int repLen = 0;
    ChatStatus status;

    if (0 == chat_decode_user_pass(_msg, _len, user, pass))
    {
        printf("  -> Login requested for User: %s\n", user);
        status = UserMng_Login(g_userMng, user, pass, _sockfd);

        repLen = chat_encode_status_rep(repBuf, OP_LOGIN_REP, status);
        if (repLen > 0) 
        { 
            send(_sockfd, repBuf, (size_t)repLen, 0); 
        }
    }
}

static void HandleCreateGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char groupName[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    int repLen = 0;
    ChatStatus status;
    Group* newGroup = NULL;

    if (0 == chat_decode_groupname(_msg, _len, groupName))
    {
        printf("  -> Create Group requested: %s\n", groupName);

        if (!UserMng_IsLoggedIn(g_userMng, _sockfd))
        {
            repLen = chat_encode_status_rep(repBuf, OP_CREATE_GROUP_REP, ST_ERR_NOT_LOGGED_IN);
            if (repLen > 0) 
            { 
                send(_sockfd, repBuf, (size_t)repLen, 0); 
            }
            return;
        }

        status = GroupMng_CreateGroup(g_groupMng, groupName, &newGroup);

        if (ST_OK == status && NULL != newGroup)
        {
            if (ST_OK != UserMng_JoinGroup(g_userMng, _sockfd, groupName))
            {
                /* Roll back: count goes to 0, IP is reclaimed */
                GroupMng_LeaveGroup(g_groupMng, groupName);
                repLen = chat_encode_status_rep(repBuf, OP_CREATE_GROUP_REP, ST_ERR_SERVER_FULL);
            }
            else
            {
                repLen = chat_encode_group_rep_ok(repBuf, OP_CREATE_GROUP_REP,
                                                newGroup->mcast_ip, newGroup->mcast_port);
            }
        }
        else
        {
            repLen = chat_encode_status_rep(repBuf, OP_CREATE_GROUP_REP, status);
        }
        if (repLen > 0) 
        { 
            send(_sockfd, repBuf, (size_t)repLen, 0); 
        }
    }
}

static void HandleJoinGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char groupName[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    int repLen = 0;
    ChatStatus status;
    ChatStatus userStatus;
    Group* group = NULL;

    if (0 == chat_decode_groupname(_msg, _len, groupName))
    {
        printf("  -> Join Group requested: %s\n", groupName);

        if (!UserMng_IsLoggedIn(g_userMng, _sockfd))
        {
            repLen = chat_encode_status_rep(repBuf, OP_JOIN_GROUP_REP, ST_ERR_NOT_LOGGED_IN);
            if (repLen > 0) 
            { 
                send(_sockfd, repBuf, (size_t)repLen, 0); 
            }
            return;
        }

        status = GroupMng_JoinGroup(g_groupMng, groupName, &group);

        if (ST_OK == status && NULL != group)
        {
            userStatus = UserMng_JoinGroup(g_userMng, _sockfd, groupName);
            if (ST_OK == userStatus)
            {
                repLen = chat_encode_group_rep_ok(repBuf, OP_JOIN_GROUP_REP, group->mcast_ip, group->mcast_port);
            }
            else
            {
                /* Undo the member_count increment — user was already in this group */
                GroupMng_LeaveGroup(g_groupMng, groupName);
                status = userStatus;
                repLen = chat_encode_status_rep(repBuf, OP_JOIN_GROUP_REP, status);
            }
        }
        else
        {
            repLen = chat_encode_status_rep(repBuf, OP_JOIN_GROUP_REP, status);
        }
        if (repLen > 0)
        {
            send(_sockfd, repBuf, (size_t)repLen, 0);
        }
    }
}

static void HandleLeaveGroupReq(int _sockfd, const uint8_t* _msg, size_t _len)
{
    char groupName[CHAT_MAX_GROUPNAME_LEN + 1] = {0};
    uint8_t repBuf[CHAT_MAX_MSG_SIZE];
    int repLen = 0;
    ChatStatus status;

    if (0 == chat_decode_groupname(_msg, _len, groupName))
    {
        printf("  -> Leave Group requested: %s\n", groupName);

        status = UserMng_LeaveGroup(g_userMng, _sockfd, groupName);

        if (ST_OK == status)
        {
            status = GroupMng_LeaveGroup(g_groupMng, groupName);
        }

        repLen = chat_encode_status_rep(repBuf, OP_LEAVE_GROUP_REP, status);
        if (repLen > 0) 
        { 
            send(_sockfd, repBuf, (size_t)repLen, 0); 
        }
    }
}