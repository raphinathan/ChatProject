#include <stdlib.h>
#include "./inc/new_gen_dlist.h"

typedef struct Node Node;

struct Node
{
    void* m_item;
    Node* m_next;
    Node* m_prev;
};

struct List
{
    Node m_head;
    Node m_tail;
};

/* --- Internal Helper Declarations --- */
static Node* CreateNode(void* _item);
static void InternalLink(Node* _left, Node* _newNode, Node* _right);

/* --- Main Functions --- */

List* ListCreate(void)
{
    List* list;
    list = (List*)malloc(sizeof(List));

    if (NULL != list)
    {
        list->m_head.m_next = &list->m_tail;
        list->m_head.m_prev = NULL;
        list->m_tail.m_prev = &list->m_head;
        list->m_tail.m_next = NULL;
    }

    return list;
}

void ListDestroy(List** _pList, void (*_elementDestroy)(void* _item))
{
    Node* curr;
    Node* next;

    if (NULL == _pList || NULL == *_pList)
    {
        return;
    }

    curr = (*_pList)->m_head.m_next;
    while (&((*_pList)->m_tail) != curr)
    {
        next = curr->m_next;
        if (NULL != _elementDestroy)
        {
            _elementDestroy(curr->m_item);
        }
        free(curr);
        curr = next;
    }

    free(*_pList);
    *_pList = NULL;
}

ListItr ListPushHead(List* _list, void* _item)
{
    if (NULL == _list)
    {
        return NULL;
    }
    return ListItrInsertBefore(_list->m_head.m_next, _item);
}

ListItr ListPushTail(List* _list, void* _item)
{
    if (NULL == _list)
    {
        return NULL;
    }
    return ListItrInsertBefore(&(_list->m_tail), _item);
}

void* ListPopHead(List* _list)
{
    if (NULL == _list || 1 == ListIsEmpty(_list))
    {
        return NULL;
    }
    return ListItrRemove(ListItrBegin(_list));
}

void* ListPopTail(List* _list)
{
    if (NULL == _list || 1 == ListIsEmpty(_list))
    {
        return NULL;
    }
    return ListItrRemove(ListItrPrev(ListItrEnd(_list)));
}

ListItr ListItrBegin(const List* _list)
{
    return (NULL == _list) ? NULL : (ListItr)_list->m_head.m_next;
}

ListItr ListItrEnd(const List* _list)
{
    return (NULL == _list) ? NULL : (ListItr)&(_list->m_tail);
}

ListItr ListItrNext(ListItr _itr)
{
    Node* n;
    n = (Node*)_itr;
    return (NULL == n->m_next) ? _itr : (ListItr)n->m_next;
}

ListItr ListItrPrev(ListItr _itr)
{
    Node* n;
    n = (Node*)_itr;
    return (NULL == n->m_prev->m_prev) ? _itr : (ListItr)n->m_prev;
}

void* ListItrGet(ListItr _itr)
{
    Node* n;
    n = (Node*)_itr;
    return (NULL == n->m_next) ? NULL : n->m_item;
}

void* ListItrSet(ListItr _itr, void* _element)
{
    Node* n;
    void* oldItem;

    n = (Node*)_itr;

    if (NULL == n->m_next)
    {
        return NULL;
    }

    oldItem = n->m_item;
    n->m_item = _element;

    return oldItem;
}

ListItr ListItrInsertBefore(ListItr _itr, void* _element)
{
    Node* newNode;
    Node* dest;

    dest = (Node*)_itr;
    newNode = CreateNode(_element);

    if (NULL != newNode)
    {
        InternalLink(dest->m_prev, newNode, dest);
    }

    return (ListItr)newNode;
}

void* ListItrRemove(ListItr _itr)
{
    Node* n;
    void* item;

    n = (Node*)_itr;
    if (NULL == n->m_next || NULL == n->m_prev)
    {
        return NULL;
    }

    item = n->m_item;
    n->m_prev->m_next = n->m_next;
    n->m_next->m_prev = n->m_prev;
    
    free(n);
    return item;
}

size_t ListIsEmpty(List* _list)
{
    return (NULL == _list || _list->m_head.m_next == &(_list->m_tail)) ? 1 : 0;
}

ListItr ListItrForEach(ListItr _begin, ListItr _end, ListActionFunction _action, void* _context)
{
    ListItr curr;
    curr = _begin;

    while (curr != _end)
    {
        if (0 == _action(ListItrGet(curr), _context))
        {
            return curr;
        }
        curr = ListItrNext(curr);
    }
    return _end;
}

/* --- Internal Helper Definitions --- */

static Node* CreateNode(void* _item)
{
    Node* newNode;
    newNode = (Node*)malloc(sizeof(Node));
    if (NULL != newNode)
    {
        newNode->m_item = _item;
    }
    return newNode;
}

static void InternalLink(Node* _left, Node* _newNode, Node* _right)
{
    _newNode->m_next = _right;
    _newNode->m_prev = _left;
    _left->m_next = _newNode;
    _right->m_prev = _newNode;
}
