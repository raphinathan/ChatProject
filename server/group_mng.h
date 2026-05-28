#ifndef GROUP_MNG_H
#define GROUP_MNG_H

#include "group.h"
#include "free_mc_queue.h"
#include "../shared/protocol.h"

typedef struct GroupMng GroupMng;

GroupMng* GroupMng_Create(FreeMcQueue* _pool);
void GroupMng_Destroy(GroupMng** _mng);

ChatStatus GroupMng_CreateGroup(GroupMng* _mng, const char* _groupName, Group** _outGroup);
ChatStatus GroupMng_JoinGroup(GroupMng* _mng, const char* _groupName, Group** _outGroup);
ChatStatus GroupMng_LeaveGroup(GroupMng* _mng, const char* _groupName);

#endif /* GROUP_MNG_H */