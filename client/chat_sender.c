/* =========================================================================
 *  chat_sender - standalone UDP multicast sender.
 *
 *  Usage: ./chat_sender <mcast_ip> <port>
 *
 *  Reads stdin line by line; each line is sent as one UDP datagram to the
 *  given multicast group. TTL is 1 (LAN only).
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

#define MAX_LINE 1024

int main(int argc, char* argv[])
{
    const char*        mcast_ip;
    int                port;
    int                sockfd;
    struct sockaddr_in dst;
    unsigned char      ttl = 1;
    char               line[MAX_LINE];

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

    fprintf(stderr, "[chat_sender] -> %s:%d (type messages, Ctrl-D to quit)\n",
            mcast_ip, port);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        size_t len = strlen(line);
        /* Keep trailing '\n' so the receiver prints clean lines. */
        if (sendto(sockfd, line, len, 0,
                   (struct sockaddr*)&dst, sizeof(dst)) < 0) {
            perror("sendto");
            break;
        }
    }

    close(sockfd);
    return 0;
}
