/* Round-trip tests for the auth subset of the TLV codec.
 * Builds standalone (links only protocol.o). Returns non-zero on any failure. */
#include "protocol.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            printf("  FAIL: %s  (line %d)\n", #cond, __LINE__);          \
            ++g_failures;                                                \
        }                                                                \
    } while (0)

static void test_user_pass_roundtrip(ChatOpcode op, const char* name)
{
    uint8_t buf[CHAT_MAX_MSG_SIZE];
    char    user[CHAT_MAX_USERNAME_LEN + 1];
    char    pass[CHAT_MAX_PASSWORD_LEN + 1];
    uint8_t t, l;
    int     n;

    printf("- %s req round-trip\n", name);
    n = chat_encode_user_pass_req(buf, op, "alice", "s3cret");
    CHECK(n > 0);
    CHECK(chat_peek_header(buf, (size_t)n, &t, &l) == 0);
    CHECK(t == op);
    CHECK((size_t)l + 2 == (size_t)n);

    CHECK(chat_decode_user_pass(buf, (size_t)n, user, pass) == 0);
    CHECK(strcmp(user, "alice") == 0);
    CHECK(strcmp(pass, "s3cret") == 0);
}

static void test_status_roundtrip(ChatOpcode op, ChatStatus st, const char* name)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    ChatStatus out;
    uint8_t    t, l;
    int        n;

    printf("- %s status round-trip (code=%d)\n", name, (int)st);
    n = chat_encode_status_rep(buf, op, st);
    CHECK(n == 3);
    CHECK(chat_peek_header(buf, (size_t)n, &t, &l) == 0);
    CHECK(t == op && l == 1);
    CHECK(chat_decode_status_rep(buf, (size_t)n, &out) == 0);
    CHECK(out == st);
}

static void test_malformed(void)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE] = {0};
    char       user[CHAT_MAX_USERNAME_LEN + 1];
    char       pass[CHAT_MAX_PASSWORD_LEN + 1];
    ChatStatus st;
    int        n;

    printf("- malformed / truncated input rejected\n");

    /* peek with < 2 bytes */
    CHECK(chat_peek_header(buf, 1, &(uint8_t){0}, &(uint8_t){0}) == -1);

    /* well-formed frame, but truncated buffer (header says more than present) */
    n = chat_encode_user_pass_req(buf, OP_REG_REQ, "bob", "pw");
    CHECK(n > 0);
    CHECK(chat_decode_user_pass(buf, (size_t)n - 1, user, pass) == -1);

    /* inner length byte lies: claims a 200-byte username inside a 10-byte buf */
    buf[0] = OP_REG_REQ;
    buf[1] = 8;
    buf[2] = 200;                  /* bogus username length */
    CHECK(chat_decode_user_pass(buf, 10, user, pass) == -1);

    /* status rep with wrong inner length */
    buf[0] = OP_LOGIN_REP;
    buf[1] = 2;                    /* must be 1 */
    buf[2] = ST_OK;
    CHECK(chat_decode_status_rep(buf, 3, &st) == -1);

    /* over-long username at encode time is rejected */
    {
        char big[CHAT_MAX_USERNAME_LEN + 5];
        memset(big, 'x', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        CHECK(chat_encode_user_pass_req(buf, OP_REG_REQ, big, "pw") == -1);
    }
}

static void test_groupname_roundtrip(ChatOpcode op, const char* name)
{
    uint8_t buf[CHAT_MAX_MSG_SIZE];
    char    group[CHAT_MAX_GROUPNAME_LEN + 1];
    uint8_t t, l;
    int     n;

    printf("- %s groupname req round-trip\n", name);
    n = chat_encode_groupname_req(buf, op, "general");
    CHECK(n > 0);
    CHECK(chat_peek_header(buf, (size_t)n, &t, &l) == 0);
    CHECK(t == op);
    CHECK(chat_decode_groupname(buf, (size_t)n, group) == 0);
    CHECK(strcmp(group, "general") == 0);
}

static void test_group_rep_ok_roundtrip(void)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    char       ip[CHAT_MAX_IP_STR_LEN];
    uint16_t   port;
    ChatStatus st;
    int        n;

    printf("- group success rep round-trip (ip + big-endian port)\n");
    n = chat_encode_group_rep_ok(buf, OP_JOIN_GROUP_REP, "239.1.1.7", 6007);
    CHECK(n > 0);
    CHECK(chat_decode_group_rep_ok(buf, (size_t)n, &st, ip, &port) == 0);
    CHECK(st == ST_OK);
    CHECK(strcmp(ip, "239.1.1.7") == 0);
    CHECK(port == 6007);                       /* proves hi/lo byte order */
}

static void test_group_rep_failure_form(void)
{
    uint8_t    buf[CHAT_MAX_MSG_SIZE];
    char       ip[CHAT_MAX_IP_STR_LEN];
    uint16_t   port;
    ChatStatus st;
    int        n;

    printf("- group failure rep (plain status) decoded without ip/port\n");
    /* Server sends a plain status reply on failure; decode_group_rep_ok
     * must accept that form and not try to read ip/port. */
    n = chat_encode_status_rep(buf, OP_JOIN_GROUP_REP, ST_ERR_GROUP_NOT_FOUND);
    CHECK(n == 3);
    CHECK(chat_decode_group_rep_ok(buf, (size_t)n, &st, ip, &port) == 0);
    CHECK(st == ST_ERR_GROUP_NOT_FOUND);
    CHECK(ip[0] == '\0');
    CHECK(port == 0);
}

static void test_logout_req(void)
{
    uint8_t buf[CHAT_MAX_MSG_SIZE];
    uint8_t t, l;
    int     n;

    printf("- logout req (empty payload)\n");
    n = chat_encode_logout_req(buf);
    CHECK(n == 2);
    CHECK(chat_peek_header(buf, (size_t)n, &t, &l) == 0);
    CHECK(t == OP_LOGOUT_REQ && l == 0);
}

static void test_group_malformed(void)
{
    uint8_t  buf[CHAT_MAX_MSG_SIZE];
    char     group[CHAT_MAX_GROUPNAME_LEN + 1];
    char     ip[CHAT_MAX_IP_STR_LEN];
    uint16_t port;
    ChatStatus st;
    int      n;

    printf("- malformed group frames rejected\n");

    /* truncated groupname req */
    n = chat_encode_groupname_req(buf, OP_CREATE_GROUP_REQ, "team");
    CHECK(n > 0);
    CHECK(chat_decode_groupname(buf, (size_t)n - 1, group) == -1);

    /* success rep claiming a longer ip than present */
    n = chat_encode_group_rep_ok(buf, OP_JOIN_GROUP_REP, "239.1.1.7", 6007);
    CHECK(n > 0);
    CHECK(chat_decode_group_rep_ok(buf, (size_t)n - 1, &st, ip, &port) == -1);

    /* over-long group name at encode time rejected */
    {
        char big[CHAT_MAX_GROUPNAME_LEN + 5];
        memset(big, 'g', sizeof(big) - 1);
        big[sizeof(big) - 1] = '\0';
        CHECK(chat_encode_groupname_req(buf, OP_CREATE_GROUP_REQ, big) == -1);
    }
}

int main(void)
{
    printf("=== protocol tests (auth + groups) ===\n");

    test_user_pass_roundtrip(OP_REG_REQ,   "REG");
    test_user_pass_roundtrip(OP_LOGIN_REQ, "LOGIN");

    test_status_roundtrip(OP_REG_REP,   ST_OK,                "REG_REP");
    test_status_roundtrip(OP_REG_REP,   ST_ERR_USER_EXISTS,   "REG_REP");
    test_status_roundtrip(OP_LOGIN_REP, ST_ERR_BAD_PASSWORD,  "LOGIN_REP");
    test_status_roundtrip(OP_LOGIN_REP, ST_ERR_USER_NOT_FOUND,"LOGIN_REP");

    test_groupname_roundtrip(OP_CREATE_GROUP_REQ, "CREATE");
    test_groupname_roundtrip(OP_JOIN_GROUP_REQ,   "JOIN");
    test_groupname_roundtrip(OP_LEAVE_GROUP_REQ,  "LEAVE");
    test_group_rep_ok_roundtrip();
    test_group_rep_failure_form();
    test_logout_req();

    test_malformed();
    test_group_malformed();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
