#include "client_net.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
/*raw TCP: connect, send all bytes, receive one message*/
/* ------------------------------------------------------------------------- */
int client_net_connect(const char* ip, uint16_t port)
{
    int                fd;
    struct sockaddr_in addr;

    if (!ip) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        fprintf(stderr, "client_net: bad server IP '%s'\n", ip);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

/* ------------------------------------------------------------------------- */
int client_net_send_all(int fd, const uint8_t* buf, size_t len)
{
    size_t sent = 0;

    if (fd < 0 || !buf) {
        return -1;
    }
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            perror("send");
            return -1;
        }
        if (n == 0) {
            return -1;             /* peer closed mid-send */
        }
        sent += (size_t)n;
    }
    return 0;
}

/* Read exactly need bytes into buf. 0 on success, -1 on EOF/error. */
static int recv_exact(int fd, uint8_t* buf, size_t need)
{
    size_t got = 0;

    while (got < need) {
        ssize_t n = recv(fd, buf + got, need - got, 0);
        if (n < 0) {
            perror("recv");
            return -1;
        }
        if (n == 0) {
            return -1;             /* server closed the connection */
        }
        got += (size_t)n;
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
int client_net_recv_msg(int fd, uint8_t* buf, size_t* out_len)
{
    uint8_t type, len;

    if (fd < 0 || !buf || !out_len) {
        return -1;
    }

    /* header: [T][L] */
    if (recv_exact(fd, buf, 2) != 0) {
        return -1;
    }
    if (chat_peek_header(buf, 2, &type, &len) != 0) {
        return -1;
    }

    /* payload: L bytes (may be 0) */
    if (len > 0 && recv_exact(fd, buf + 2, len) != 0) {
        return -1;
    }

    *out_len = (size_t)2 + len;
    return 0;
}

/* ------------------------------------------------------------------------- */
void client_net_close(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}
