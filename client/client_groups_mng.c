#include "client_groups_mng.h"
#include "child_msg.h"
#include "../shared/adt/gen_dlist.h"
#include "../shared/protocol.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>

/* Per-joined-group entry: stored as a List element. */
typedef struct {
    char  name[CHAT_MAX_GROUPNAME_LEN + 1];
    pid_t sender_pid;
    pid_t receiver_pid;
} GroupEntry;

struct ClientGroupsMng {
    int   msqid;     /* SysV message queue id (-1 if create failed) */
    List* groups;    /* of GroupEntry* */
};

/* ------------------------------------------------------------------------- */
static void destroy_entry(void* p)
{
    free(p);
}

/* Find iterator pointing at the entry named group_name, or end iter. */
static ListItr find_entry(List* list, const char* group_name)
{
    ListItr itr = ListItrBegin(list);
    ListItr end = ListItrEnd(list);

    while (itr != end) {
        GroupEntry* e = (GroupEntry*)ListItrGet(itr);
        if (e && strcmp(e->name, group_name) == 0) {
            return itr;
        }
        itr = ListItrNext(itr);
    }
    return end;
}

/* Block on the queue until a message of the given mtype arrives; return
 * the embedded PID. Returns -1 on msgrcv failure. */
static pid_t recv_child_pid(int msqid, long mtype)
{
    ChatChildMsg msg;
    if (msgrcv(msqid, &msg, sizeof(msg.pid), mtype, 0) < 0) {
        perror("msgrcv");
        return -1;
    }
    return msg.pid;
}

static void kill_entry(GroupEntry* e)
{
    if (!e) {
        return;
    }
    if (e->sender_pid > 0) {
        kill(e->sender_pid, SIGTERM);
    }
    if (e->receiver_pid > 0) {
        kill(e->receiver_pid, SIGTERM);
    }
}

/* ------------------------------------------------------------------------- */
ClientGroupsMng* client_groups_mng_create(void)
{
    ClientGroupsMng* m;
    key_t            key;

    m = calloc(1, sizeof(*m));
    if (!m) {
        return NULL;
    }
    m->msqid = -1;

    /* Derive a per-client queue key from our own PID (low byte as proj_id). */
    key = ftok(CHAT_CHILD_FTOK_PATH, (int)(getpid() & 0xff));
    if (key == (key_t)-1) {
        perror("ftok");
        free(m);
        return NULL;
    }

    m->msqid = msgget(key, IPC_CREAT | 0600);
    if (m->msqid < 0) {
        perror("msgget");
        free(m);
        return NULL;
    }

    m->groups = ListCreate();
    if (!m->groups) {
        msgctl(m->msqid, IPC_RMID, NULL);
        free(m);
        return NULL;
    }
    return m;
}

/* ------------------------------------------------------------------------- */
void client_groups_mng_destroy(ClientGroupsMng** pm)
{
    if (!pm || !*pm) {
        return;
    }
    client_groups_mng_clear(*pm);
    if ((*pm)->groups) {
        ListDestroy(&(*pm)->groups, destroy_entry);
    }
    if ((*pm)->msqid >= 0) {
        msgctl((*pm)->msqid, IPC_RMID, NULL);
    }
    free(*pm);
    *pm = NULL;
}

/* ------------------------------------------------------------------------- */
int client_groups_mng_on_join(ClientGroupsMng* m, const char* group_name,
                              const char* mcast_ip, uint16_t mcast_port,
                              const char* username)
{
    GroupEntry* e;
    char        cmd[256];

    if (!m || !group_name || !mcast_ip) {
        return -1;
    }
    if (!username) {
        username = "";
    }
    if (find_entry(m->groups, group_name) != ListItrEnd(m->groups)) {
        /* Already tracked locally -- shouldn't happen (server rejects). */
        return -1;
    }

    /* Spawn the two windows. gnome-terminal forks into the background, so
     * system() returns once the launch is dispatched and we can msgrcv.
     * The sender gets the username so it can tag outgoing messages. */
    snprintf(cmd, sizeof(cmd),
             "gnome-terminal --title='%s (sender)' "
             "-- ./bin/chat_sender %s %u %d '%s' &",
             group_name, mcast_ip, (unsigned)mcast_port, m->msqid, username);
    if (system(cmd) != 0) {
        fprintf(stderr, "client_groups_mng: failed to spawn chat_sender\n");
        return -1;
    }
    snprintf(cmd, sizeof(cmd),
             "gnome-terminal --title='%s (receiver)' "
             "-- ./bin/chat_receiver %s %u %d '%s' &",
             group_name, mcast_ip, (unsigned)mcast_port, m->msqid, group_name);
    if (system(cmd) != 0) {
        fprintf(stderr, "client_groups_mng: failed to spawn chat_receiver\n");
        return -1;
    }

    e = calloc(1, sizeof(*e));
    if (!e) {
        return -1;
    }
    strncpy(e->name, group_name, CHAT_MAX_GROUPNAME_LEN);
    e->name[CHAT_MAX_GROUPNAME_LEN] = '\0';

    /* mtype filter lets us read each role independently regardless of which
     * window finished startup first. */
    e->sender_pid   = recv_child_pid(m->msqid, CHAT_CHILD_MTYPE_SENDER);
    e->receiver_pid = recv_child_pid(m->msqid, CHAT_CHILD_MTYPE_RECEIVER);

    if (!ListPushTail(m->groups, e)) {
        kill_entry(e);
        free(e);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
void client_groups_mng_on_leave(ClientGroupsMng* m, const char* group_name)
{
    ListItr     itr;
    GroupEntry* e;

    if (!m || !group_name) {
        return;
    }
    itr = find_entry(m->groups, group_name);
    if (itr == ListItrEnd(m->groups)) {
        return;
    }
    e = (GroupEntry*)ListItrRemove(itr);
    if (!e) {
        return;
    }
    kill_entry(e);
    free(e);
}

/* ------------------------------------------------------------------------- */
void client_groups_mng_clear(ClientGroupsMng* m)
{
    GroupEntry* e;

    if (!m || !m->groups) {
        return;
    }
    while ((e = (GroupEntry*)ListPopHead(m->groups)) != NULL) {
        kill_entry(e);
        free(e);
    }
}
