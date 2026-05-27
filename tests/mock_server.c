/* =========================================================================
 *  THROWAWAY mock server - speaks the real TLV wire format so the client can
 *  be tested for register/login today, without the partner's real server.
 *
 *  Keeps users in a plain in-memory array (NO HashMap on purpose - this file
 *  is disposable and gets deleted once server/ handles auth for real).
 *  Single connection at a time; sequential accept loop is fine for testing.
 *
 *  The reply behavior here IS the spec the real server must match:
 *    REG:   exists -> ST_ERR_USER_EXISTS,        else add -> ST_OK
 *    LOGIN: missing -> ST_ERR_USER_NOT_FOUND, bad pw -> ST_ERR_BAD_PASSWORD,
 *           else -> ST_OK
 * ========================================================================= */
#include "protocol.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_USERS 64

typedef struct {
    char name[CHAT_MAX_USERNAME_LEN + 1];
    char pass[CHAT_MAX_PASSWORD_LEN + 1];
} MockUser;

static MockUser g_users[MAX_USERS];
static int      g_user_count = 0;

static MockUser* find_user(const char* name)
{
    for (int i = 0; i < g_user_count; ++i) {
        if (strcmp(g_users[i].name, name) == 0) {
            return &g_users[i];
        }
    }
    return NULL;
}

static ChatStatus handle_register(const char* user, const char* pass)
{
    if (find_user(user)) {
        return ST_ERR_USER_EXISTS;
    }
    if (g_user_count >= MAX_USERS) {
        return ST_ERR_SERVER_FULL;
    }
    strncpy(g_users[g_user_count].name, user, CHAT_MAX_USERNAME_LEN);
    g_users[g_user_count].name[CHAT_MAX_USERNAME_LEN] = '\0';
    strncpy(g_users[g_user_count].pass, pass, CHAT_MAX_PASSWORD_LEN);
    g_users[g_user_count].pass[CHAT_MAX_PASSWORD_LEN] = '\0';
    ++g_user_count;
    return ST_OK;
}

static ChatStatus handle_login(const char* user, const char* pass)
{
    MockUser* u = find_user(user);
    if (!u) {
        return ST_ERR_USER_NOT_FOUND;
    }
    if (strcmp(u->pass, pass) != 0) {
        return ST_ERR_BAD_PASSWORD;
    }
    return ST_OK;
}

/* Read exactly need bytes; 0 on success, -1 on EOF/error. */
static int recv_exact(int fd, uint8_t* buf, size_t need)
{
    size_t got = 0;
    while (got < need) {
        ssize_t n = recv(fd, buf + got, need - got, 0);
        if (n <= 0) {
            return -1;
        }
        got += (size_t)n;
    }
    return 0;
}

static int send_all(int fd, const uint8_t* buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, buf + sent, len - sent, 0);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

/* Serve one connected client until it disconnects. */
static void serve_client(int cfd)
{
    uint8_t buf[CHAT_MAX_MSG_SIZE];

    for (;;) {
        uint8_t    type, len;
        char       user[CHAT_MAX_USERNAME_LEN + 1];
        char       pass[CHAT_MAX_PASSWORD_LEN + 1];
        ChatStatus status;
        ChatOpcode rep_op;
        int        n;

        if (recv_exact(cfd, buf, 2) != 0) {
            printf("[mock] client disconnected\n");
            return;
        }
        if (chat_peek_header(buf, 2, &type, &len) != 0) {
            printf("[mock] bad header, closing\n");
            return;
        }
        if (len > 0 && recv_exact(cfd, buf + 2, len) != 0) {
            printf("[mock] truncated payload, closing\n");
            return;
        }

        printf("[mock] recv opcode 0x%02X (len=%u)\n", type, len);

        if (type == OP_REG_REQ || type == OP_LOGIN_REQ) {
            if (chat_decode_user_pass(buf, (size_t)2 + len, user, pass) != 0) {
                status = ST_ERR_PROTOCOL;
            } else if (type == OP_REG_REQ) {
                status = handle_register(user, pass);
            } else {
                status = handle_login(user, pass);
            }
            rep_op = (type == OP_REG_REQ) ? OP_REG_REP : OP_LOGIN_REP;
        } else {
            printf("[mock] unsupported opcode 0x%02X\n", type);
            status = ST_ERR_PROTOCOL;
            rep_op = OP_REG_REP;   /* best-effort reply */
        }

        n = chat_encode_status_rep(buf, rep_op, status);
        if (n < 0 || send_all(cfd, buf, (size_t)n) != 0) {
            printf("[mock] send failed, closing\n");
            return;
        }
        printf("[mock] reply status 0x%02X\n", status);
    }
}

int main(void)
{
    int                lfd, opt = 1;
    struct sockaddr_in addr;

    setvbuf(stdout, NULL, _IOLBF, 0);  /* flush log per line, even when piped */

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
        perror("socket");
        return 1;
    }
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(CHAT_TCP_PORT);

    if (bind(lfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(lfd);
        return 1;
    }
    if (listen(lfd, 4) < 0) {
        perror("listen");
        close(lfd);
        return 1;
    }

    printf("[mock] listening on port %d (Ctrl-C to stop)\n", CHAT_TCP_PORT);
    for (;;) {
        int cfd = accept(lfd, NULL, NULL);
        if (cfd < 0) {
            perror("accept");
            continue;
        }
        printf("[mock] client connected\n");
        serve_client(cfd);
        close(cfd);
    }
    /* not reached */
}
