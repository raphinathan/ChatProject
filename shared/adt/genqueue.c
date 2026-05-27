#include <stdlib.h>  /* malloc, free */
#include "genqueue.h"

#define QUEUE_MAGIC_NUMBER 0xBEEFCAFE

struct Queue {
    void**  m_items;        /* circular buffer array */
    size_t  m_size;         /* total capacity of the buffer */
    size_t  m_head;         /* index of the front element */
    size_t  m_tail;         /* index of the next insertion point */
    size_t  m_numOfElements;/* current number of elements */
    unsigned int m_magic;   /* magic number to prevent double free */
};

Queue* QueueCreate(size_t _size)
{
    Queue* queue;

    if (_size == 0) {
        return NULL;
    }

    queue = (Queue*)malloc(sizeof(Queue));
    if (queue == NULL) {
        return NULL;
    }

    queue->m_items = (void**)malloc(_size * sizeof(void*));
    if (queue->m_items == NULL) {
        free(queue);
        return NULL;
    }

    queue->m_size          = _size;
    queue->m_head          = 0;
    queue->m_tail          = 0;
    queue->m_numOfElements = 0;
    queue->m_magic         = QUEUE_MAGIC_NUMBER;

    return queue;
}

void QueueDestroy(Queue** _queue, DestroyItem _itemDestroy)
{
    size_t i, idx;

    if (_queue == NULL || *_queue == NULL || (*_queue)->m_magic != QUEUE_MAGIC_NUMBER) {
        return;
    }

    if (_itemDestroy != NULL) {
        idx = (*_queue)->m_head;
        for (i = 0; i < (*_queue)->m_numOfElements; ++i) {
            _itemDestroy((*_queue)->m_items[idx]);
            idx = (idx + 1) % (*_queue)->m_size;
        }
    }

    free((*_queue)->m_items);
    (*_queue)->m_magic = 0;
    free(*_queue);
    *_queue = NULL;
}

QueueResult QueueInsert(Queue* _queue, void* _item)
{
    if (_queue == NULL || _queue->m_magic != QUEUE_MAGIC_NUMBER) {
        return QUEUE_UNINITIALIZED_ERROR;
    }

    if (_item == NULL) {
        return QUEUE_DATA_UNINITIALIZED_ERROR;
    }

    if (_queue->m_numOfElements == _queue->m_size) {
        return QUEUE_OVERFLOW_ERROR;  /* queue is full */
    }

    _queue->m_items[_queue->m_tail] = _item;
    _queue->m_tail = (_queue->m_tail + 1) % _queue->m_size;
    _queue->m_numOfElements++;

    return QUEUE_SUCCESS;
}

QueueResult QueueRemove(Queue* _queue, void** _item)
{
    if (_queue == NULL || _queue->m_magic != QUEUE_MAGIC_NUMBER) {
        return QUEUE_UNINITIALIZED_ERROR;
    }

    if (_item == NULL) {
        return QUEUE_DATA_UNINITIALIZED_ERROR;
    }

    if (_queue->m_numOfElements == 0) {
        return QUEUE_DATA_NOT_FOUND_ERROR;  /* queue is empty */
    }

    *_item = _queue->m_items[_queue->m_head];
    _queue->m_head = (_queue->m_head + 1) % _queue->m_size;
    _queue->m_numOfElements--;

    return QUEUE_SUCCESS;
}

size_t QueueIsEmpty(Queue* _queue)
{
    if (_queue == NULL || _queue->m_magic != QUEUE_MAGIC_NUMBER) {
        return 1;  /* treat invalid queue as empty */
    }
    return (_queue->m_numOfElements == 0) ? 1 : 0;
}

size_t QueueForEach(Queue* _queue, ActionFunction _action, void* _context)
{
    size_t i, idx, count = 0;

    if (_queue == NULL || _queue->m_magic != QUEUE_MAGIC_NUMBER || _action == NULL) {
        return 0;
    }

    idx = _queue->m_head;
    for (i = 0; i < _queue->m_numOfElements; ++i) {
        ++count;
        if (_action(_queue->m_items[idx], _context) == 0) {
            break;
        }
        idx = (idx + 1) % _queue->m_size;
    }

    return count;
}
