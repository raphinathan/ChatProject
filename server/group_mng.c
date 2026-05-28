#include "group_mng.h"
#include "hash_utils.h"
#include "../shared/adt/HashMap.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct GroupMng 
{
    HashMap* groupsByName;
    FreeMcQueue* ipPool; /* Reference to the external queue */
};

/* --- Helper Function Declarations --- */
static void DestroyGroupCallback(void* _value);

/* --- Main Functions --- */

GroupMng* GroupMng_Create(FreeMcQueue* _pool)
{
    GroupMng* mng = NULL;

    if (NULL == _pool)
    {
        return NULL;
    }

    mng = (GroupMng*)malloc(sizeof(GroupMng));
    if (NULL == mng)
    {
        return NULL;
    }

    mng->groupsByName = HashMap_Create(100, HashStringDjb2, StringKeysEqual);
    if (NULL == mng->groupsByName)
    {
        free(mng);
        return NULL;
    }

    mng->ipPool = _pool;
    return mng;
}

void GroupMng_Destroy(GroupMng** _mng)
{
    if (NULL == _mng || NULL == *_mng)
    {
        return;
    }

    HashMap_Destroy(&((*_mng)->groupsByName), NULL, DestroyGroupCallback);
    
    free(*_mng);
    *_mng = NULL;
}

ChatStatus GroupMng_CreateGroup(GroupMng* _mng, const char* _groupName, Group** _outGroup)
{
    Group* newGroup = NULL;
    McastPair pair;
    void* dummyVal = NULL;

    if (NULL == _mng || NULL == _groupName || NULL == _outGroup)
    {
        return ST_ERR_PROTOCOL;
    }

    /* 1. Ensure group name is unique */
    if (MAP_SUCCESS == HashMap_Find(_mng->groupsByName, (const void*)_groupName, &dummyVal))
    {
        return ST_ERR_GROUP_EXISTS;
    }

    /* 2. Pull a fresh Multicast IP from the pool */
    if (0 == FreeMcQueue_Take(_mng->ipPool, &pair))
    {
        return ST_ERR_SERVER_FULL; /* No more IPs available in the queue */
    }

    /* 3. Allocate and initialize the Group */
    newGroup = (Group*)malloc(sizeof(Group));
    if (NULL == newGroup)
    {
        FreeMcQueue_Return(_mng->ipPool, &pair); /* Undo IP assignment on failure */
        return ST_ERR_PROTOCOL;
    }

    strncpy(newGroup->name, _groupName, CHAT_MAX_GROUPNAME_LEN);
    newGroup->name[CHAT_MAX_GROUPNAME_LEN] = '\0';
    
    strcpy(newGroup->mcast_ip, pair.ip);
    newGroup->mcast_port = pair.port;
    newGroup->member_count = 1; /* The creator automatically joins */

    /* 4. Insert into the Map */
    if (MAP_SUCCESS != HashMap_Insert(_mng->groupsByName, newGroup->name, newGroup))
    {
        FreeMcQueue_Return(_mng->ipPool, &pair);
        free(newGroup);
        return ST_ERR_PROTOCOL;
    }

    *_outGroup = newGroup;
    return ST_OK;
}

ChatStatus GroupMng_JoinGroup(GroupMng* _mng, const char* _groupName, Group** _outGroup)
{
    Group* group = NULL;

    if (NULL == _mng || NULL == _groupName || NULL == _outGroup)
    {
        return ST_ERR_PROTOCOL;
    }

    if (MAP_SUCCESS != HashMap_Find(_mng->groupsByName, (const void*)_groupName, (void**)&group))
    {
        return ST_ERR_GROUP_NOT_FOUND;
    }

    group->member_count++;
    *_outGroup = group;

    return ST_OK;
}

ChatStatus GroupMng_LeaveGroup(GroupMng* _mng, const char* _groupName)
{
    Group* group = NULL;
    void* pKey = NULL;
    void* pVal = NULL;
    McastPair pair;

    if (NULL == _mng || NULL == _groupName)
    {
        return ST_ERR_PROTOCOL;
    }

    if (MAP_SUCCESS != HashMap_Find(_mng->groupsByName, (const void*)_groupName, (void**)&group))
    {
        return ST_ERR_GROUP_NOT_FOUND;
    }

    if (group->member_count > 0)
    {
        group->member_count--;
    }

    /* Garbage Collection: If the group is empty, destroy it and reclaim the IP */
    if (0 == group->member_count)
    {
        printf("Group '%s' is empty. Reclaiming Multicast IP %s.\n", group->name, group->mcast_ip);
        
        strcpy(pair.ip, group->mcast_ip);
        pair.port = group->mcast_port;
        FreeMcQueue_Return(_mng->ipPool, &pair);

        HashMap_Remove(_mng->groupsByName, _groupName, &pKey, &pVal);
        DestroyGroupCallback(pVal);
    }

    return ST_OK;
}

/* --- Helper Function Definitions --- */

static void DestroyGroupCallback(void* _value)
{
    if (NULL != _value)
    {
        free(_value);
    }
}