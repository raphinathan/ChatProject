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

/* Screen 2 placeholder until the groups phase lands. */
static void screen2_stub(ClientMng* m)
{
    printf("\n--- logged in as '%s' ---\n", client_mng_username(m));
    printf("Group features (create / join / leave) arrive in the next phase.\n");
    printf("Press Enter to log out...\n");
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF) {
            /* wait for Enter */
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
                screen2_stub(m);
                /* Back to screen 1 after the stub for now. */
            }
        } else if (strcmp(choice, "3") == 0) {
            printf("bye.\n");
            return;
        } else {
            printf("  please enter 1, 2, or 3\n");
        }
    }
}
