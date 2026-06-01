/* =========================================================================
 *  chat_receiver - standalone UDP multicast receiver.
 *
 *  Usage: ./chat_receiver <mcast_ip> <port> [msqid] [group_name]
 *
 *  Joins the given multicast group on the default interface and prints
 *  every datagram to stdout. Multiple receivers can share the same port
 *  (SO_REUSEADDR) so two terminals on the same host both receive.
 *
 *  Phase 5 will extend this to also msgsnd(getpid()) back to the parent
 *  client via a SysV message queue; for now it's purely a UDP tool so we
 *  can verify multicast plumbing standalone.
 * ========================================================================= */

#include "child_msg.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_DGRAM 1500

int main(int argc, char* argv[])
{
    const char*        mcast_ip;
    int                port;
    int                msqid = -1;
    const char*        group_name = NULL;
    int                sockfd;
    int                reuse = 1;
    struct sockaddr_in bind_addr;
    struct ip_mreq     mreq;
    char               buf[MAX_DGRAM + 1];

    if (argc < 3 || argc > 5) {
        fprintf(stderr, "usage: %s <mcast_ip> <port> [msqid] [group_name]\n",
                argv[0]);
        return 1;
    }
    mcast_ip = argv[1];
    port     = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "bad port: %s\n", argv[2]);
        return 1;
    }
    if (argc >= 4) {
        msqid = atoi(argv[3]);
    }
    if (argc == 5 && argv[4][0] != '\0') {
        group_name = argv[4];
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    /* Let multiple receivers share the port (two windows on one host). */
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(sockfd);
        return 1;
    }

    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port   = htons((uint16_t)port);
    /* Bind to the group address (not INADDR_ANY) so the kernel filters by
     * destination: with several groups sharing one port, each receiver only
     * gets its own group's traffic instead of every group's (Linux). */
    if (inet_pton(AF_INET, mcast_ip, &bind_addr.sin_addr) != 1) {
        fprintf(stderr, "bad multicast ip: %s\n", mcast_ip);
        close(sockfd);
        return 1;
    }
    if (bind(sockfd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    memset(&mreq, 0, sizeof(mreq));
    if (inet_pton(AF_INET, mcast_ip, &mreq.imr_multiaddr) != 1) {
        fprintf(stderr, "bad multicast ip: %s\n", mcast_ip);
        close(sockfd);
        return 1;
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt IP_ADD_MEMBERSHIP");
        close(sockfd);
        return 1;
    }

    /* If launched by the client, hand our PID back over the msgq so the
     * parent can kill() us on leave/logout. Non-fatal on failure. */
    if (msqid >= 0) {
        ChatChildMsg msg;
        msg.mtype = CHAT_CHILD_MTYPE_RECEIVER;
        msg.pid   = getpid();
        if (msgsnd(msqid, &msg, sizeof(msg.pid), 0) < 0) {
            perror("msgsnd");
        }
    }

    if (group_name) {
        fprintf(stderr, "[chat_receiver] group \"%s\" -- joined %s:%d, "
                "waiting...\n", group_name, mcast_ip, port);
    } else {
        fprintf(stderr, "[chat_receiver] joined %s:%d, waiting...\n",
                mcast_ip, port);
    }

    for (;;) {
        ssize_t n = recvfrom(sockfd, buf, MAX_DGRAM, 0, NULL, NULL);
        if (n < 0) {
            perror("recvfrom");
            break;
        }
        buf[n] = '\0';
        fputs(buf, stdout);
        fflush(stdout);
    }

    /* Best-effort: drop membership before close. */
    setsockopt(sockfd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq));
    close(sockfd);
    return 0;
}
