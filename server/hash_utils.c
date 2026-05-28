#include "hash_utils.h"

#include <string.h> /* strcmp */

size_t HashStringDjb2(void* _key)
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

int StringKeysEqual(void* _firstKey, void* _secondKey)
{
    if (NULL == _firstKey || NULL == _secondKey)
    {
        return 0;
    }
    return (0 == strcmp((char*)_firstKey, (char*)_secondKey)) ? 1 : 0;
}
