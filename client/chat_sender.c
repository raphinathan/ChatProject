/* =========================================================================
 *  chat_sender - standalone UDP multicast sender.
 *
 *  Usage: ./chat_sender <mcast_ip> <port> [msqid] [username]
 *
 *  Reads stdin line by line; each line is sent as one UDP datagram to the
 *  given multicast group. TTL is 1 (LAN only). When a username is given,
 *  each line is prefixed with "<username>: " so receivers can see who sent
 *  it.
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

#define MAX_LINE 1024
#define MAX_NAME 33  /* CHAT_MAX_USERNAME_LEN (32) + 1 */

int main(int argc, char* argv[])
{
    const char*        mcast_ip;
    int                port;
    int                msqid = -1;
    const char*        username = NULL;
    int                sockfd;
    struct sockaddr_in dst;
    unsigned char      ttl = 1;
    char               line[MAX_LINE];
    char               out[MAX_NAME + 2 + MAX_LINE]; /* "<name>: " + line */

    if (argc < 3 || argc > 5) {
        fprintf(stderr, "usage: %s <mcast_ip> <port> [msqid] [username]\n",
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
        username = argv[4];
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_TTL,
                   &ttl, sizeof(ttl)) < 0) {
        perror("setsockopt IP_MULTICAST_TTL");
        close(sockfd);
        return 1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, mcast_ip, &dst.sin_addr) != 1) {
        fprintf(stderr, "bad multicast ip: %s\n", mcast_ip);
        close(sockfd);
        return 1;
    }

    /* If launched by the client, hand our PID back over the msgq so the
     * parent can kill() us on leave/logout. Non-fatal on failure. */
    if (msqid >= 0) {
        ChatChildMsg msg;
        msg.mtype = CHAT_CHILD_MTYPE_SENDER;
        msg.pid   = getpid();
        if (msgsnd(msqid, &msg, sizeof(msg.pid), 0) < 0) {
            perror("msgsnd");
        }
    }

    fprintf(stderr, "[chat_sender] -> %s:%d (type messages, Ctrl-D to quit)\n",
            mcast_ip, port);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        const char* payload = line;
        size_t      len;

        /* Prefix "<username>: " so receivers can attribute the message.
         * Keep the trailing '\n' so the receiver prints clean lines. */
        if (username) {
            int w = snprintf(out, sizeof(out), "%s: %s", username, line);
            if (w > 0) {
                payload = out;
            }
        }
        len = strlen(payload);
        if (sendto(sockfd, payload, len, 0,
                   (struct sockaddr*)&dst, sizeof(dst)) < 0) {
            perror("sendto");
            break;
        }
    }

    close(sockfd);
    return 0;
}
