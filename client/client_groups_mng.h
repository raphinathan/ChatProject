#ifndef CHAT_CLIENT_GROUPS_MNG_H
#define CHAT_CLIENT_GROUPS_MNG_H

#include <stdint.h>

/* =========================================================================
 *  Client Groups Mng - owns the SysV message queue used to receive child
 *  PIDs from spawned chat_sender / chat_receiver windows, and a list
 *  mapping each joined group to its (sender_pid, receiver_pid) pair so we
 *  can kill() the windows on leave / logout.
 * ========================================================================= */

typedef struct ClientGroupsMng ClientGroupsMng;

/* Create the queue (msgget IPC_CREAT) and the group list. Returns NULL on
 * any allocation / IPC failure. */
ClientGroupsMng* client_groups_mng_create(void);

/* Kill all remaining child windows, drop the queue (msgctl IPC_RMID),
 * free the struct. Safe on NULL / *pm == NULL. */
void             client_groups_mng_destroy(ClientGroupsMng** pm);

/* Spawn sender + receiver windows for this group via gnome-terminal, then
 * block on msgrcv twice to collect both child PIDs, and store them under
 * group_name. Returns 0 on success, -1 on failure (group not registered).
 *
 * Best-effort: if spawning fails the caller is still considered joined
 * server-side -- the user will see the failure in stderr and can leave. */
int  client_groups_mng_on_join (ClientGroupsMng* m, const char* group_name,
                                const char* mcast_ip, uint16_t mcast_port);

/* SIGTERM both children for this group, then drop the entry. No-op if
 * group_name is not registered (e.g. spawn previously failed). */
void client_groups_mng_on_leave(ClientGroupsMng* m, const char* group_name);

/* Kill every registered child and clear the list. Used on logout, when
 * the server has already marked us out of all groups. */
void client_groups_mng_clear   (ClientGroupsMng* m);

#endif /* CHAT_CLIENT_GROUPS_MNG_H */
