/* server/user_mng.c */

#include "user_mng.h"
#include "user.h"
#include "../shared/adt/HashMap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h> /* For FD_SETSIZE */

/* --- Internal Data Structures --- */

struct UserMng 
{
    /* Primary index: maps char* (username) to User* */
    HashMap* usersByName; 
    
    /* Secondary index: maps int (socket FD) to User* for instant disconnect handling */
    User* activeSockets[FD_SETSIZE]; 
};

/* --- Helper Function Declarations --- */

static size_t HashStringDjb2(void* _key);
static int KeysAreEqual(void* _firstKey, void* _secondKey);
static void DestroyUserCallback(void* _value);

/* --- Main Functions --- */

UserMng* UserMng_Create(void)
{
    UserMng* mng = NULL;
    int i = 0;

    mng = (UserMng*)malloc(sizeof(UserMng));
    if (NULL == mng)
    {
        return NULL;
    }

    /* Anticipating 100 max users. The HashMap will automatically round up to a prime. */
    mng->usersByName = HashMap_Create(100, HashStringDjb2, KeysAreEqual);
    if (NULL == mng->usersByName)
    {
        free(mng);
        return NULL;
    }

    /* Initialize the socket index array to NULL */
    for (i = 0; i < FD_SETSIZE; ++i)
    {
        mng->activeSockets[i] = NULL;
    }

    return mng;
}

void UserMng_Destroy(UserMng** _mng)
{
    if (NULL == _mng || NULL == *_mng)
    {
        return;
    }

    /* * HashMap_Destroy takes function pointers to safely free the keys and values.
     * We pass NULL for the key because the key is pointing to the User->username 
     * string, which is already stored INSIDE the User struct. Freeing the User 
     * struct frees the string. We pass our custom callback to free the User.
     */
    HashMap_Destroy(&((*_mng)->usersByName), NULL, DestroyUserCallback);
    
    free(*_mng);
    *_mng = NULL;
}

ChatStatus UserMng_Register(UserMng* _mng, const char* _username, const char* _password)
{
    User* newUser = NULL;
    void* dummyVal = NULL;

    if (NULL == _mng || NULL == _username || NULL == _password)
    {
        return ST_ERR_PROTOCOL;
    }

    /* 1. Check if the username is already taken */
    if (MAP_SUCCESS == HashMap_Find(_mng->usersByName, (const void*)_username, &dummyVal))
    {
        return ST_ERR_USER_EXISTS;
    }

    /* 2. Allocate the new User entity */
    newUser = (User*)malloc(sizeof(User));
    if (NULL == newUser)
    {
        perror("malloc failed for User");
        return ST_ERR_SERVER_FULL;
    }

    /* 3. Initialize fields securely */
    strncpy(newUser->username, _username, CHAT_MAX_USERNAME_LEN);
    newUser->username[CHAT_MAX_USERNAME_LEN] = '\0';

    strncpy(newUser->password, _password, CHAT_MAX_PASSWORD_LEN);
    newUser->password[CHAT_MAX_PASSWORD_LEN] = '\0';

    newUser->state = USER_OFFLINE;
    newUser->socketFd = -1;
    newUser->joinedGroups = ListCreate();

    if (NULL == newUser->joinedGroups)
    {
        free(newUser);
        return ST_ERR_SERVER_FULL;
    }

    /* 4. Insert into the HashMap. The key points to the string inside the struct. */
    if (MAP_SUCCESS != HashMap_Insert(_mng->usersByName, newUser->username, newUser))
    {
        ListDestroy(&(newUser->joinedGroups), NULL);
        free(newUser);
        return ST_ERR_SERVER_FULL;
    }

    return ST_OK;
}

ChatStatus UserMng_Login(UserMng* _mng, const char* _username, const char* _password, int _sockfd)
{
    User* user = NULL;

    if (NULL == _mng || NULL == _username || NULL == _password || 0 > _sockfd || _sockfd >= FD_SETSIZE )
    {
        return ST_ERR_PROTOCOL;
    }

    /* Ensure this socket doesn't already have a logged-in user on it */
    if (NULL != _mng->activeSockets[_sockfd])
    {
        return ST_ERR_ALREADY_LOGGED_IN;
    }

    /* 1. Lookup User */
    if (MAP_SUCCESS != HashMap_Find(_mng->usersByName, (const void*)_username, (void**)&user))
    {
        return ST_ERR_USER_NOT_FOUND;
    }

    /* 2. Verify Password */
    if (0 != strcmp(user->password, _password))
    {
        return ST_ERR_BAD_PASSWORD;
    }

    /* 3. Check State */
    if (USER_ONLINE == user->state)
    {
        return ST_ERR_ALREADY_LOGGED_IN;
    }

    /* 4. Update State and Secondary Index */
    user->state = USER_ONLINE;
    user->socketFd = _sockfd;
    _mng->activeSockets[_sockfd] = user;

    return ST_OK;
}

ChatStatus UserMng_Logout(UserMng* _mng, int _sockfd)
{
    User* user = NULL;

    if (NULL == _mng || 0 > _sockfd || _sockfd >= FD_SETSIZE)
    {
        return ST_ERR_PROTOCOL;
    }

    user = _mng->activeSockets[_sockfd];
    
    if (NULL == user)
    {
        return ST_ERR_NOT_LOGGED_IN;
    }

    /* Update State and clear Secondary Index */
    user->state = USER_OFFLINE;
    user->socketFd = -1;
    _mng->activeSockets[_sockfd] = NULL;

    /* (Phase 3: We will also iterate through joinedGroups here to leave them) */

    return ST_OK;
}

void UserMng_Disconnect(UserMng* _mng, int _sockfd)
{
    /* Disconnect does exactly what logout does, but silently. */
    (void)UserMng_Logout(_mng, _sockfd);  /* not logged in = nothing to do */
}

/* --- Helper Function Definitions --- */

static size_t HashStringDjb2(void* _key)
{
    unsigned char* str = (unsigned char*)_key;
    size_t hash = 5381;
    int c = 0;

    if (NULL == str) 
    {
        return 0;
    }

    while ((c = *str++)) 
    {
        hash = ((hash << 5) + hash) + (size_t)c; /* hash * 33 + c */
    }

    return hash;
}

static int KeysAreEqual(void* _firstKey, void* _secondKey)
{
    if (NULL == _firstKey || NULL == _secondKey) 
    {
        return 0;
    }
    
    if (0 == strcmp((char*)_firstKey, (char*)_secondKey))
    {
        return 1;
    }
    
    return 0;
}

static void DestroyUserCallback(void* _value)
{
    User* user = (User*)_value;
    
    if (NULL != user)
    {
        if (NULL != user->joinedGroups)
        {
            ListDestroy(&(user->joinedGroups), NULL); /* Requires a valid destroy func in Phase 3 */
        }
        free(user);
    }
}