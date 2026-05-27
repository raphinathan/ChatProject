#include "ui.h"

#include <stdio.h>
#include <string.h>

/* Read a line into out (size cap), strip trailing newline, drain any overflow.
 * Returns 0 on success, -1 on EOF. */
static int read_line(const char* prompt, char* out, size_t cap)
{
    printf("%s", prompt);
    fflush(stdout);

    if (!fgets(out, (int)cap, stdin)) {
        return -1;                 /* EOF / error */
    }

    size_t len = strlen(out);
    if (len > 0 && out[len - 1] == '\n') {
        out[len - 1] = '\0';
    } else {
        /* line longer than cap: discard the rest so it doesn't bleed in */
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            /* drain */
        }
    }
    return 0;
}

/* Human-readable message for a status returned by an auth request. */
static const char* status_msg(ChatStatus st)
{
    switch (st) {
        case ST_OK:                    return "OK";
        case ST_ERR_USER_EXISTS:       return "username already taken";
        case ST_ERR_USER_NOT_FOUND:    return "no such user";
        case ST_ERR_BAD_PASSWORD:      return "wrong password";
        case ST_ERR_ALREADY_LOGGED_IN: return "already logged in";
        case ST_ERR_NOT_LOGGED_IN:     return "not logged in";
        case ST_ERR_GROUP_EXISTS:      return "group name already taken";
        case ST_ERR_GROUP_NOT_FOUND:   return "no such group";
        case ST_ERR_ALREADY_IN_GROUP:  return "already in that group";
        case ST_ERR_NOT_IN_GROUP:      return "not a member of that group";
        case ST_ERR_SERVER_FULL:       return "server is full";
        case ST_ERR_PROTOCOL:          return "connection / protocol error";
        default:                       return "unexpected error";
    }
}

/* Prompt for username + password, run req, report result. Returns the status. */
static ChatStatus prompt_and_auth(ClientMng* m, int is_login)
{
    char user[CHAT_MAX_USERNAME_LEN + 1];
    char pass[CHAT_MAX_PASSWORD_LEN + 1];
    ChatStatus st;

    if (read_line("  username: ", user, sizeof(user)) != 0 ||
        read_line("  password: ", pass, sizeof(pass)) != 0) {
        return ST_ERR_PROTOCOL;
    }
    if (user[0] == '\0') {
        printf("  username cannot be empty\n");
        return ST_ERR_PROTOCOL;
    }

    st = is_login ? client_mng_login(m, user, pass)
                  : client_mng_register(m, user, pass);

    if (st == ST_OK) {
        printf("  %s succeeded\n", is_login ? "login" : "registration");
    } else {
        printf("  %s failed: %s\n", is_login ? "login" : "registration",
               status_msg(st));
    }
    return st;
}

/* Prompt for a group name and run a create/join. On success print the
 * multicast (ip, port) the server assigned (no chat windows yet). */
static void do_group_join(ClientMng* m, int is_create)
{
    char     group[CHAT_MAX_GROUPNAME_LEN + 1];
    char     ip[CHAT_MAX_IP_STR_LEN];
    uint16_t port;
    ChatStatus st;
    const char* verb = is_create ? "create" : "join";

    if (read_line("  group name: ", group, sizeof(group)) != 0) {
        return;
    }
    if (group[0] == '\0') {
        printf("  group name cannot be empty\n");
        return;
    }

    st = is_create ? client_mng_create_group(m, group, ip, &port)
                   : client_mng_join_group(m, group, ip, &port);

    if (st == ST_OK) {
        printf("  %s '%s' OK -- multicast %s:%u\n", verb, group, ip, port);
    } else {
        printf("  %s '%s' failed: %s\n", verb, group, status_msg(st));
    }
}

static void do_group_leave(ClientMng* m)
{
    char       group[CHAT_MAX_GROUPNAME_LEN + 1];
    ChatStatus st;

    if (read_line("  group name: ", group, sizeof(group)) != 0) {
        return;
    }
    if (group[0] == '\0') {
        printf("  group name cannot be empty\n");
        return;
    }

    st = client_mng_leave_group(m, group);
    if (st == ST_OK) {
        printf("  left '%s'\n", group);
    } else {
        printf("  leave '%s' failed: %s\n", group, status_msg(st));
    }
}

/* Screen 2: group operations. Returns when the user logs out (or input ends). */
static void screen2(ClientMng* m)
{
    char choice[8];

    for (;;) {
        printf("\n=== Groups (logged in as '%s') ===\n", client_mng_username(m));
        printf("  1) create group\n");
        printf("  2) join group\n");
        printf("  3) leave group\n");
        printf("  4) logout\n");

        if (read_line("> ", choice, sizeof(choice)) != 0) {
            return;                    /* input closed; ui_run handles exit */
        }

        if (strcmp(choice, "1") == 0) {
            do_group_join(m, 1 /* create */);
        } else if (strcmp(choice, "2") == 0) {
            do_group_join(m, 0 /* join */);
        } else if (strcmp(choice, "3") == 0) {
            do_group_leave(m);
        } else if (strcmp(choice, "4") == 0) {
            ChatStatus st = client_mng_logout(m);
            if (st == ST_OK) {
                printf("  logged out\n");
                return;                /* back to screen 1 */
            }
            printf("  logout failed: %s\n", status_msg(st));
        } else {
            printf("  please enter 1, 2, 3, or 4\n");
        }
    }
}

void ui_run(ClientMng* m)
{
    char choice[8];

    for (;;) {
        printf("\n=== Chat ===\n");
        printf("  1) register\n");
        printf("  2) login\n");
        printf("  3) exit\n");

        if (read_line("> ", choice, sizeof(choice)) != 0) {
            printf("\ninput closed, exiting.\n");
            return;
        }

        if (strcmp(choice, "1") == 0) {
            prompt_and_auth(m, 0 /* register */);
        } else if (strcmp(choice, "2") == 0) {
            if (prompt_and_auth(m, 1 /* login */) == ST_OK) {
                screen2(m);            /* returns here on logout */
            }
        } else if (strcmp(choice, "3") == 0) {
            printf("bye.\n");
            return;
        } else {
            printf("  please enter 1, 2, or 3\n");
        }
    }
}
