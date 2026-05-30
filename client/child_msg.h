#ifndef CHAT_CHILD_MSG_H
#define CHAT_CHILD_MSG_H

#include <sys/types.h>   /* pid_t */

/* =========================================================================
 *  SysV message-queue protocol between the client and the chat_sender /
 *  chat_receiver windows it spawns.
 *
 *  On startup each child window sends ONE message to the queue carrying
 *  its getpid(), tagged with its role. The parent reads exactly two
 *  messages per join (one of each type) so it can later kill() the
 *  children on leave / logout.
 *
 *  The queue key is derived from the parent client's PID so two clients
 *  on the same host get distinct queues:
 *      msqid = msgget(ftok(CHAT_CHILD_FTOK_PATH, ppid & 0xff),
 *                     IPC_CREAT | 0600);
 * ========================================================================= */

#define CHAT_CHILD_FTOK_PATH       "/tmp"
#define CHAT_CHILD_MTYPE_SENDER    1L
#define CHAT_CHILD_MTYPE_RECEIVER  2L

typedef struct {
    long  mtype;            /* CHAT_CHILD_MTYPE_SENDER or _RECEIVER */
    pid_t pid;
} ChatChildMsg;

#endif /* CHAT_CHILD_MSG_H */
