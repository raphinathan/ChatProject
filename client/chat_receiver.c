/* =========================================================================
 *  chat_receiver - standalone UDP multicast receiver.
 *
 *  Usage: ./chat_receiver <mcast_ip> <port>
 *
 *  Joins the given multicast group on the default interface and prints
 *  every datagram to stdout. Multiple receivers can share the same port
 *  (SO_REUSEADDR) so two terminals on the same host both receive.
 *
 *  Phase 5 will extend this to also msgsnd(getpid()) back to the parent
 *  client via a SysV message queue; for now it's purely a UDP tool so we
 *  can verify multicast plumbing standalone.
 * ========================================================================= */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_DGRAM 1500

int main(int argc, char* argv[])
{
    const char*        mcast_ip;
    int                port;
    int                sockfd;
    int                reuse = 1;
    struct sockaddr_in bind_addr;
    struct ip_mreq     mreq;
    char               buf[MAX_DGRAM + 1];

    if (argc != 3) {
        fprintf(stderr, "usage: %s <mcast_ip> <port>\n", argv[0]);
        return 1;
    }
    mcast_ip = argv[1];
    port     = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "bad port: %s\n", argv[2]);
        return 1;
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
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons((uint16_t)port);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
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

    fprintf(stderr, "[chat_receiver] joined %s:%d, waiting...\n",
            mcast_ip, port);

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
