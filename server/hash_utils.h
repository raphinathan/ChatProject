#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include <stddef.h> /* size_t */

/* DJB2 hash for null-terminated string keys. */
size_t HashStringDjb2(void* _key);

/* Case-sensitive string equality for HashMap key comparison. */
int StringKeysEqual(void* _firstKey, void* _secondKey);

#endif /* HASH_UTILS_H */
