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

int main(void)
{
    printf("=== protocol auth-subset tests ===\n");

    test_user_pass_roundtrip(OP_REG_REQ,   "REG");
    test_user_pass_roundtrip(OP_LOGIN_REQ, "LOGIN");

    test_status_roundtrip(OP_REG_REP,   ST_OK,                "REG_REP");
    test_status_roundtrip(OP_REG_REP,   ST_ERR_USER_EXISTS,   "REG_REP");
    test_status_roundtrip(OP_LOGIN_REP, ST_ERR_BAD_PASSWORD,  "LOGIN_REP");
    test_status_roundtrip(OP_LOGIN_REP, ST_ERR_USER_NOT_FOUND,"LOGIN_REP");

    test_malformed();

    if (g_failures == 0) {
        printf("\nALL TESTS PASSED\n");
        return 0;
    }
    printf("\n%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
