#include "server_net.h"
#include "../shared/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>

/* FD_SETSIZE is defined by the OS (usually 1024). It is the maximum number 
 * of file descriptors that select() can monitor simultaneously. */
#define MAX_CLIENTS FD_SETSIZE

/* We need a buffer larger than a single message to handle TCP fragmentation.
 * If 2.5 messages arrive at once, we need space to store the overlap. */
#define RECV_BUFFER_SIZE (CHAT_MAX_MSG_SIZE * 3)

/* --- Internal Data Structures --- */

typedef struct 
{
    int fd;
    uint8_t buffer[RECV_BUFFER_SIZE];
    size_t len; /* Tracks exactly how many unparsed bytes are currently in the buffer */
} ClientState;

/* --- Helper Function Declarations --- */

static int SetupListeningSocket(uint16_t _port);
static void HandleNewConnection(int _listenFd, ClientState* _clients, int* _maxFd, fd_set* _masterSet);
static void HandleClientData(ClientState* _client, fd_set* _masterSet, OnMessageFn _onMsg, OnDisconnectFn _onDisc, void* _ctx);
static void ProcessBuffer(ClientState* _client, OnMessageFn _onMsg, void* _ctx);

/* --- Main Functions --- */

int ServerNet_Run(uint16_t _port, OnMessageFn _onMsg, OnDisconnectFn _onDisc,
                  void* _ctx, const volatile sig_atomic_t* _keepRunning)
{
    int listenFd = -1;
    int maxFd = 0;
    int i = 0;
    int cleanExit = 1;
    
    /* fd_set is a bit-array used by select() to know which sockets to monitor. */
    fd_set masterSet; 
    fd_set readSet;
    static ClientState clients[MAX_CLIENTS];

    /* Initialize all client slots to -1 (empty) */
    for (i = 0; i < MAX_CLIENTS; ++i)
    {
        clients[i].fd = -1;
        clients[i].len = 0;
    }

    listenFd = SetupListeningSocket(_port);
    if (-1 == listenFd)
    {
        return -1;
    }

    /* Clear the master set and add our listening socket to it. */
    FD_ZERO(&masterSet);
    FD_SET(listenFd, &masterSet);
    maxFd = listenFd; /* select() needs the highest FD number to know where to stop iterating */

    printf("Server listening on port %d...\n", _port);

    while (*_keepRunning)
    {
        /* select() modifies the set passed into it. We must pass a copy (readSet)
         * so we don't lose our master list of connected clients. */
        readSet = masterSet;

        /* Blocks until at least one FD in readSet is ready for reading */
        if (-1 == select(maxFd + 1, &readSet, NULL, NULL, NULL))
        {
            if (errno == EINTR)
            {
                continue; /* signal fired — re-check *_keepRunning at top of loop */
            }
            perror("select failed");
            cleanExit = 0;
            break;
        }

        /* 1. If the listening socket is readable, a new client is trying to connect. */
        if (FD_ISSET(listenFd, &readSet))
        {
            HandleNewConnection(listenFd, clients, &maxFd, &masterSet);
        }

        /* 2. Check all existing client sockets to see if they sent data. */
        for (i = 0; i < MAX_CLIENTS; ++i)
        {
            if (-1 != clients[i].fd && FD_ISSET(clients[i].fd, &readSet))
            {
                HandleClientData(&clients[i], &masterSet, _onMsg, _onDisc, _ctx);
            }
        }
    }

    close(listenFd);
    return cleanExit ? 0 : -1;

}

/* --- Helper Function Definitions --- */

static int SetupListeningSocket(uint16_t _port)
{
    int listenFd = -1;
    int optval = 1;
    struct sockaddr_in servAddr;

    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == listenFd)
    {
        return -1;
    }

    /* SO_REUSEADDR prevents the "Address already in use" error if you restart 
     * the server quickly. It tells the OS to forcefully reclaim the port. */
    if (-1 == setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)))
    {
        close(listenFd);
        return -1;
    }

    memset(&servAddr, 0, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Bind to all available network interfaces */
    servAddr.sin_port = htons(_port); /* Host-To-Network-Short (Big Endian conversion) */

    if (-1 == bind(listenFd, (struct sockaddr*)&servAddr, sizeof(servAddr)))
    {
        close(listenFd);
        return -1;
    }

    /* The '10' is the backlog: how many pending connections the OS will queue 
     * before dropping them if our select() loop is busy. */
    if (-1 == listen(listenFd, 10))
    {
        close(listenFd);
        return -1;
    }

    return listenFd;
}

static void HandleNewConnection(int _listenFd, ClientState* _clients, int* _maxFd, fd_set* _masterSet)
{
    int newFd = -1;
    int i = 0;
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);

    newFd = accept(_listenFd, (struct sockaddr*)&clientAddr, &addrLen);
    if (-1 == newFd)
    {
        return;
    }

    /* Find an empty slot in the array. O(N) lookup is acceptable here because 
     * MAX_CLIENTS is typically small (1024) and connections are infrequent. */
    for (i = 0; i < MAX_CLIENTS; ++i)
    {
        if (-1 == _clients[i].fd)
        {
            _clients[i].fd = newFd;
            _clients[i].len = 0; 
            
            /* Add the new client to the master set so select() monitors it */
            FD_SET(newFd, _masterSet);
            if (newFd > *_maxFd)
            {
                *_maxFd = newFd;
            }
            return;
        }
    }

    /* No slots left. We must actively close the socket to reject them. */
    close(newFd);
}

static void HandleClientData(ClientState* _client, fd_set* _masterSet, OnMessageFn _onMsg, OnDisconnectFn _onDisc, void* _ctx)
{
    ssize_t bytesRead = 0;
    
    /* Calculate remaining space to prevent buffer overflow */
    size_t spaceLeft = RECV_BUFFER_SIZE - _client->len;
    if (spaceLeft == 0) 
    { 
        /* protocol error: drop client */ 
        return; 
    }

    /* Read directly into the buffer, offset by whatever data is already there */
    bytesRead = recv(_client->fd, _client->buffer + _client->len, spaceLeft, 0);

    /* recv() returns 0 if the client elegantly called close() on their end. 
     * It returns -1 if the connection dropped unexpectedly (e.g., power loss). */
    if (0 >= bytesRead) 
    {
        close(_client->fd);
        FD_CLR(_client->fd, _masterSet); /* Stop monitoring this socket */
        
        if (NULL != _onDisc)
        {
            _onDisc(_client->fd, _ctx); /* Inform the Management layer */
        }
        
        _client->fd = -1; /* Mark slot as free */
        _client->len = 0;
    }
    else
    {
        _client->len += (size_t)bytesRead;
        ProcessBuffer(_client, _onMsg, _ctx);
    }
}

static void ProcessBuffer(ClientState* _client, OnMessageFn _onMsg, void* _ctx)
{
    uint8_t type = 0;
    uint8_t length = 0;
    size_t totalMsgLen = 0;

    /* Loop repeatedly. One recv() call might contain multiple distinct TLV messages. */
    while (1)
    {
        /* If peek_header returns -1, we have fewer than 2 bytes. 
         * We don't even know the payload length yet. Break and wait for more data. */
        if (-1 == chat_peek_header(_client->buffer, _client->len, &type, &length))
        {
            break; 
        }

        totalMsgLen = 2 + (size_t)length; /* 1 byte Type + 1 byte Length + Payload */

        /* We know how long the message *should* be, but we haven't received it all yet. 
         * Break and let the select() loop fetch the rest later. */
        if (_client->len < totalMsgLen)
        {
            break; 
        }

        /* We have a full, valid message. Fire the callback. */
        if (NULL != _onMsg)
        {
            _onMsg(_client->fd, _client->buffer, totalMsgLen, _ctx);
        }

        _client->len -= totalMsgLen;
        
        if (_client->len > 0)
        {
            /* memmove is CRITICAL here, not memcpy. 
             * We are shifting remaining unparsed bytes to the front of the buffer. 
             * Because the source and destination memory regions overlap, memcpy 
             * causes undefined behavior. memmove safely handles overlapping memory. */
            memmove(_client->buffer, _client->buffer + totalMsgLen, _client->len);
        }
    }
}