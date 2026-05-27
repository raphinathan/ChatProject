#include <stdio.h>

#include "client_mng.h"
#include "protocol.h"
#include "ui.h"

int main(int argc, char** argv)
{
    const char* server_ip = (argc > 1) ? argv[1] : "127.0.0.1";
    ClientMng*  m;

    printf("connecting to %s:%d ...\n", server_ip, CHAT_TCP_PORT);
    m = client_mng_create(server_ip, CHAT_TCP_PORT);
    if (!m) {
        fprintf(stderr, "could not connect to server at %s:%d\n",
                server_ip, CHAT_TCP_PORT);
        return 1;
    }
    printf("connected.\n");

    ui_run(m);

    client_mng_destroy(&m);
    return 0;
}
