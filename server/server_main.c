#include "server_net.h"
#include "server_mng.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* The 'volatile' keyword tells the compiler: "This variable can change 
 * unexpectedly outside of the current code flow (e.g., inside an interrupt)." 
 * This prevents the compiler's optimizer from replacing g_keepRunning with '1' 
 * and creating an infinite loop. */
static volatile int g_keepRunning = 1;

/* Signal handler function. Must take an int representing the signal number. */
static void SigIntHandler(int _sig)
{
    (void)_sig;
    g_keepRunning = 0;
}

int main(void)
{
    /* Bind SIGINT (the signal sent when you press Ctrl+C) to our handler. 
     * Now, Ctrl+C won't immediately kill the program. It sets the flag, allowing 
     * ServerNet_Run to eventually exit gracefully. */
    signal(SIGINT, SigIntHandler);

    if (0 == ServerMng_Init())
    {
        return EXIT_FAILURE;
    }

    /* Blocks here until ServerNet_Run returns (which happens if select() fails, 
     * or if we modified ServerNet_Run to check g_keepRunning). */
    ServerNet_Run(CHAT_TCP_PORT, ServerMng_HandleMessage, ServerMng_OnDisconnect, NULL);

    /* This will only execute if the network loop breaks, allowing you 
     * to safely call HashMap_Destroy inside ServerMng_Destroy. */
    ServerMng_Destroy();
    
    return EXIT_SUCCESS;
}