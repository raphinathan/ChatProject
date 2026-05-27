#include <stdlib.h> /* malloc, calloc, free */
#include "./inc/HashMap.h"
#include "./inc/new_gen_dlist.h"

/* --- Internal Structures --- */
struct HashMap
{
    List** buckets;
    size_t capacity;
    size_t size;
    HashFunction hashFunc;
    EqualityFunction keysEqualFunc;
};

typedef struct KeyValuePair
{
    const void* key;
    void* value;
} KeyValuePair;

/* --- Helper Function Declarations --- */
static size_t GetNextPrime(size_t _num);
static int IsPrime(size_t _num);
static size_t GetBucketIndex(const HashMap* _map, const void* _key);
static KeyValuePair* CreateKeyValuePair(const void* _key, const void* _value);
static void DestroyKeyValuePair(KeyValuePair* _kvp);
static KeyValuePair* FindInList(List* _list, const void* _key, EqualityFunction _isEqual, ListItr* _outItr);
static void DestroySingleBucket(List* _bucket, void (*_keyDestroy)(void*), void (*_valDestroy)(void*));

/* --- Main API Implementations --- */

HashMap* HashMap_Create(size_t _capacity, HashFunction _hashFunc, EqualityFunction _keysEqualFunc)
{
    HashMap* map = NULL;
    size_t primeCapacity = 0;

    if (NULL == _hashFunc || NULL == _keysEqualFunc || 0 == _capacity)
    {
        return NULL;
    }

    map = (HashMap*)malloc(sizeof(HashMap));
    if (NULL == map)
    {
        return NULL;
    }

    primeCapacity = GetNextPrime(_capacity);

    map->buckets = (List**)calloc(primeCapacity, sizeof(List*));
    if (NULL == map->buckets)
    {
        free(map);
        return NULL;
    }

    map->capacity = primeCapacity;
    map->size = 0;
    map->hashFunc = _hashFunc;
    map->keysEqualFunc = _keysEqualFunc;

    return map;
}

void HashMap_Destroy(HashMap** _map, void (*_keyDestroy)(void* _key), void (*_valDestroy)(void* _value))
{
    size_t i = 0;

    if (NULL == _map || NULL == *_map)
    {
        return;
    }

    for (i = 0; i < (*_map)->capacity; ++i)
    {
        if (NULL != (*_map)->buckets[i])
        {
            DestroySingleBucket((*_map)->buckets[i], _keyDestroy, _valDestroy);
        }
    }

    free((*_map)->buckets);
    free(*_map);
    *_map = NULL;
}

Map_Result HashMap_Insert(HashMap* _map, const void* _key, const void* _value)
{
    size_t index = 0;
    KeyValuePair* kvp = NULL;
    ListItr dummyItr = NULL;

    if (NULL == _map)
    {
        return MAP_UNINITIALIZED_ERROR;
    }
    if (NULL == _key)
    {
        return MAP_KEY_NULL_ERROR;
    }

    index = GetBucketIndex(_map, _key);

    if (NULL == _map->buckets[index])
    {
        _map->buckets[index] = ListCreate();
        if (NULL == _map->buckets[index])
        {
            return MAP_ALLOCATION_ERROR;
        }
    }
    else
    {
        if (NULL != FindInList(_map->buckets[index], _key, _map->keysEqualFunc, &dummyItr))
        {
            return MAP_KEY_DUPLICATE_ERROR;
        }
    }

    kvp = CreateKeyValuePair(_key, _value);
    if (NULL == kvp)
    {
        return MAP_ALLOCATION_ERROR;
    }

    if (NULL == ListPushTail(_map->buckets[index], kvp))
    {
        DestroyKeyValuePair(kvp);
        return MAP_ALLOCATION_ERROR;
    }

    _map->size++;
    return MAP_SUCCESS;
}

Map_Result HashMap_Remove(HashMap* _map, const void* _searchKey, void** _pKey, void** _pValue)
{
    size_t index = 0;
    KeyValuePair* kvp = NULL;
    ListItr targetItr = NULL;

    if (NULL == _map)
    {
        return MAP_UNINITIALIZED_ERROR;
    }
    if (NULL == _searchKey)
    {
        return MAP_KEY_NULL_ERROR;
    }

    index = GetBucketIndex(_map, _searchKey);
    
    if (NULL == _map->buckets[index])
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    kvp = FindInList(_map->buckets[index], _searchKey, _map->keysEqualFunc, &targetItr);

    if (NULL == kvp)
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    ListItrRemove(targetItr);

    if (NULL != _pKey)
    {
        *_pKey = (void*)kvp->key;
    }
    if (NULL != _pValue)
    {
        *_pValue = kvp->value;
    }

    DestroyKeyValuePair(kvp);
    _map->size--;

    return MAP_SUCCESS;
}

Map_Result HashMap_Find(const HashMap* _map, const void* _key, void** _pValue)
{
    size_t index = 0;
    KeyValuePair* kvp = NULL;

    if (NULL == _map)
    {
        return MAP_UNINITIALIZED_ERROR;
    }
    if (NULL == _key)
    {
        return MAP_KEY_NULL_ERROR;
    }

    index = GetBucketIndex(_map, _key);
    
    if (NULL == _map->buckets[index])
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    kvp = FindInList(_map->buckets[index], _key, _map->keysEqualFunc, NULL);

    if (NULL == kvp)
    {
        return MAP_KEY_NOT_FOUND_ERROR;
    }

    if (NULL != _pValue)
    {
        *_pValue = kvp->value;
    }

    return MAP_SUCCESS;
}

size_t HashMap_Size(const HashMap* _map)
{
    if (NULL == _map)
    {
        return 0;
    }
    return _map->size;
}

/* --- Helper Function Definitions --- */

static int IsPrime(size_t _num)
{
    size_t i = 0;

    if (_num <= 1)
    {
        return 0;
    }
    if (2 == _num || 3 == _num)
    {
        return 1;
    }
    if (0 == _num % 2 || 0 == _num % 3)
    {
        return 0;
    }

    for (i = 5; i * i <= _num; i += 6)
    {
        if (0 == _num % i || 0 == _num % (i + 2))
        {
            return 0;
        }
    }
    return 1;
}

static size_t GetNextPrime(size_t _num)
{
    size_t prime = _num;
    while (0 == IsPrime(prime))
    {
        prime++;
    }
    return prime;
}

static size_t GetBucketIndex(const HashMap* _map, const void* _key)
{
    size_t rawHash = 0;

    if (NULL == _map || NULL == _key)
    {
        return 0;
    }

    rawHash = _map->hashFunc((void*)_key);
    return rawHash % _map->capacity;
}

static KeyValuePair* CreateKeyValuePair(const void* _key, const void* _value)
{
    KeyValuePair* kvp = (KeyValuePair*)malloc(sizeof(KeyValuePair));
    
    if (NULL != kvp)
    {
        kvp->key = _key;
        kvp->value = (void*)_value;
    }

    return kvp;
}

static void DestroyKeyValuePair(KeyValuePair* _kvp)
{
    if (NULL != _kvp)
    {
        free(_kvp);
    }
}

static KeyValuePair* FindInList(List* _list, const void* _key, EqualityFunction _isEqual, ListItr* _outItr)
{
    ListItr itr = NULL;
    ListItr end = NULL;
    KeyValuePair* kvp = NULL;

    if (NULL == _list)
    {
        return NULL;
    }

    itr = ListItrBegin(_list);
    end = ListItrEnd(_list);

    while (itr != end)
    {
        kvp = (KeyValuePair*)ListItrGet(itr);
        if (NULL != kvp && _isEqual((void*)_key, (void*)kvp->key))
        {
            if (NULL != _outItr)
            {
                *_outItr = itr;
            }
            return kvp;
        }
        itr = ListItrNext(itr);
    }

    return NULL;
}

static void DestroySingleBucket(List* _bucket, void (*_keyDestroy)(void*), void (*_valDestroy)(void*))
{
    ListItr itr = NULL;
    ListItr end = NULL;
    KeyValuePair* kvp = NULL;

    if (NULL == _bucket)
    {
        return;
    }

    itr = ListItrBegin(_bucket);
    end = ListItrEnd(_bucket);

    while (itr != end)
    {
        kvp = (KeyValuePair*)ListItrGet(itr);
        if (NULL != kvp)
        {
            if (NULL != _keyDestroy)
            {
                _keyDestroy((void*)kvp->key);
            }
            if (NULL != _valDestroy)
            {
                _valDestroy(kvp->value);
            }
            DestroyKeyValuePair(kvp);
        }
        itr = ListItrNext(itr);
    }

    ListDestroy(&_bucket, NULL);
}
