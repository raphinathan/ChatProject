#include "client_mng.h"
#include "client_groups_mng.h"
#include "client_net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ← "do a register/login": owns the socket + login state*/
struct ClientMng {
    int               sockfd;
    char              server_ip[CHAT_MAX_IP_STR_LEN];
    uint16_t          port;
    int               logged_in;
    char              username[CHAT_MAX_USERNAME_LEN + 1];
    ClientGroupsMng*  groups;          /* msgq + spawned chat windows  */
};

/* ------------------------------------------------------------------------- */
ClientMng* client_mng_create(const char* server_ip, uint16_t port)
{
    ClientMng* m;

    if (!server_ip) {
        return NULL;
    }

    m = calloc(1, sizeof(*m)); /* why calloc? because it zeroes the memory, so we don't have to manually set all fields to zero/NULL. It's a common practice to use calloc when you want to initialize a struct with default values (like 0 for integers and NULL for pointers). */
    if (!m) {
        return NULL;
    }

    m->sockfd = client_net_connect(server_ip, port);
    if (m->sockfd < 0) {
        free(m);
        return NULL;
    }

    m->groups = client_groups_mng_create();
    if (!m->groups) {
        client_net_close(m->sockfd);
        free(m);
        return NULL;
    }

    strncpy(m->server_ip, server_ip, sizeof(m->server_ip) - 1);
    m->port       = port;
    m->logged_in  = 0;
    m->username[0] = '\0';
    return m;
}

/* ------------------------------------------------------------------------- */
void client_mng_destroy(ClientMng** pm)
{
    if (!pm || !*pm) {
        return;
    }
    /* Tear down child windows + msgq before dropping the socket; the
     * server's disconnect handler will mark us out of any remaining groups. */
    client_groups_mng_destroy(&(*pm)->groups);
    client_net_close((*pm)->sockfd);
    free(*pm);
    *pm = NULL;
}

/* Send a [user][pass] request of the given opcode, return decoded status. */
static ChatStatus do_user_pass(ClientMng* m, ChatOpcode op,
                               const char* user, const char* pass)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    size_t     rlen;
    ChatStatus status;
    int        n;

    n = chat_encode_user_pass_req(buf, op, user, pass);
    if (n < 0) {
        return ST_ERR_PROTOCOL;
    }
    if (client_net_send_all(m->sockfd, buf, (size_t)n) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (client_net_recv_msg(m->sockfd, buf, &rlen) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (chat_decode_status_rep(buf, rlen, &status) != 0) {
        return ST_ERR_PROTOCOL;
    }
    return status;
}

/* ------------------------------------------------------------------------- */
ChatStatus client_mng_register(ClientMng* m, const char* user, const char* pass)
{
    if (!m || !user || !pass) {
        return ST_ERR_PROTOCOL;
    }
    return do_user_pass(m, OP_REG_REQ, user, pass);
}

/* ------------------------------------------------------------------------- */
ChatStatus client_mng_login(ClientMng* m, const char* user, const char* pass)
{
    ChatStatus status;

    if (!m || !user || !pass) {
        return ST_ERR_PROTOCOL;
    }
    status = do_user_pass(m, OP_LOGIN_REQ, user, pass);
    if (status == ST_OK) {
        m->logged_in = 1;
        strncpy(m->username, user, sizeof(m->username) - 1);
        m->username[sizeof(m->username) - 1] = '\0';
    }
    return status;
}

/* ------------------------------------------------------------------------- */
ChatStatus client_mng_logout(ClientMng* m)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    size_t     rlen;
    ChatStatus status;
    int        n;

    if (!m) {
        return ST_ERR_PROTOCOL;
    }
    n = chat_encode_logout_req(buf);
    if (n < 0 || client_net_send_all(m->sockfd, buf, (size_t)n) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (client_net_recv_msg(m->sockfd, buf, &rlen) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (chat_decode_status_rep(buf, rlen, &status) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (status == ST_OK) {
        /* Server has dropped us from all groups; tear down local windows. */
        client_groups_mng_clear(m->groups);
        m->logged_in  = 0;
        m->username[0] = '\0';
    }
    return status;
}

/* Send a CREATE/JOIN request and decode the (ip, port)-bearing reply. */
static ChatStatus do_group_join(ClientMng* m, ChatOpcode op, const char* group,
                                char* out_ip, uint16_t* out_port)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    size_t     rlen;
    ChatStatus status;
    int        n;

    n = chat_encode_groupname_req(buf, op, group);
    if (n < 0 || client_net_send_all(m->sockfd, buf, (size_t)n) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (client_net_recv_msg(m->sockfd, buf, &rlen) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (chat_decode_group_rep_ok(buf, rlen, &status, out_ip, out_port) != 0) {
        return ST_ERR_PROTOCOL;
    }
    return status;
}

/* ------------------------------------------------------------------------- */
ChatStatus client_mng_create_group(ClientMng* m, const char* group,
                                   char* out_ip, uint16_t* out_port)
{
    ChatStatus status;

    if (!m || !group || !out_ip || !out_port) {
        return ST_ERR_PROTOCOL;
    }
    status = do_group_join(m, OP_CREATE_GROUP_REQ, group, out_ip, out_port);
    if (status == ST_OK) {
        if (client_groups_mng_on_join(m->groups, group, out_ip, *out_port,
                                      m->username) != 0) {
            fprintf(stderr, "warning: failed to open chat windows for '%s'\n",
                    group);
        }
    }
    return status;
}

/* ------------------------------------------------------------------------- */
ChatStatus client_mng_join_group(ClientMng* m, const char* group,
                                 char* out_ip, uint16_t* out_port)
{
    ChatStatus status;

    if (!m || !group || !out_ip || !out_port) {
        return ST_ERR_PROTOCOL;
    }
    status = do_group_join(m, OP_JOIN_GROUP_REQ, group, out_ip, out_port);
    if (status == ST_OK) {
        if (client_groups_mng_on_join(m->groups, group, out_ip, *out_port,
                                      m->username) != 0) {
            fprintf(stderr, "warning: failed to open chat windows for '%s'\n",
                    group);
        }
    }
    return status;
}

/* ------------------------------------------------------------------------- */
ChatStatus client_mng_leave_group(ClientMng* m, const char* group)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    size_t     rlen;
    ChatStatus status;
    int        n;

    if (!m || !group) {
        return ST_ERR_PROTOCOL;
    }
    n = chat_encode_groupname_req(buf, OP_LEAVE_GROUP_REQ, group);
    if (n < 0 || client_net_send_all(m->sockfd, buf, (size_t)n) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (client_net_recv_msg(m->sockfd, buf, &rlen) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (chat_decode_status_rep(buf, rlen, &status) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (status == ST_OK) {
        client_groups_mng_on_leave(m->groups, group);
    }
    return status;
}

/* ------------------------------------------------------------------------- */
int client_mng_is_logged_in(const ClientMng* m)
{
    return m ? m->logged_in : 0;
}

const char* client_mng_username(const ClientMng* m)
{
    return m ? m->username : "";
}
