#include <stdlib.h>
#include "gen_dlist.h"

/* ------------------------------------------------------------------
 * Internal types
 * ------------------------------------------------------------------
 * Design: circular doubly-linked list with a sentinel (dummy) node.
 *
 *   empty list:        sentinel <-> sentinel   (points to itself)
 *
 *   non-empty:         sentinel <-> A <-> B <-> C <-> sentinel
 *
 * Begin iterator = sentinel->next  (first real node, or sentinel if empty)
 * End iterator   = sentinel itself
 *
 * Why a sentinel?
 *   - No NULL checks at the boundaries (every node has valid prev/next).
 *   - Push/pop at head and tail share the same insert/remove code paths.
 *   - Iterators always point at *some* node; "end" is a real address.
 * ------------------------------------------------------------------ */

typedef struct Node {
    void*        data;
    struct Node* next;
    struct Node* prev;
    int          is_sentinel;  /* 1 only for the list's sentinel node */
} Node;

struct List {
    Node sentinel;
};

/* ------------------------------------------------------------------
 * Helpers (file-private)
 * ------------------------------------------------------------------ */

/* Create a fresh detached node with given data. */
static Node* NodeCreate(void* _data) {
    Node* n = (Node*)malloc(sizeof(Node));
    if (!n) return NULL;
    n->data        = _data;
    n->next        = NULL;
    n->prev        = NULL;
    n->is_sentinel = 0;
    return n;
}

/* Splice 'n' into the list right BEFORE node 'pos'.
 * Works whether 'pos' is the sentinel (=> insert at tail)
 * or any real node.
 */
static void NodeLinkBefore(Node* pos, Node* n) {
    n->prev   = pos->prev;
    n->next   = pos;
    pos->prev->next = n;
    pos->prev       = n;
}

/* Unlink node 'n' from its neighbours. Caller must not pass the sentinel. */
static void NodeUnlink(Node* n) {
    /*A->n->B = n->prev->n->n->next*/
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->prev = NULL;
    n->next = NULL;
}

/* ------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------ */

List* ListCreate(void) {
    List* list = (List*)malloc(sizeof(List));
    if (!list) return NULL;

    /* Initialize the sentinel to point at itself => empty list. */
    list->sentinel.data        = NULL;
    list->sentinel.next        = &list->sentinel;
    list->sentinel.prev        = &list->sentinel;
    list->sentinel.is_sentinel = 1;
    return list;
}

void ListDestroy(List** _pList, void (*_elementDestroy)(void* _item)) { /*a pointer to a function returning void*/
    if (!_pList || !*_pList) return;

    List* list = *_pList;
    Node* cur  = list->sentinel.next;

    while (cur != &list->sentinel) {
        Node* nxt = cur->next;
        if (_elementDestroy) {
            _elementDestroy(cur->data); /*This is calling the function whose address is stored in _elementDestroy, passing it cur->data (which is a void*)*/
        }
        free(cur);
        cur = nxt;
    }

    free(list);
    *_pList = NULL;
}

/* ------------------------------------------------------------------
 * Push / Pop
 * ------------------------------------------------------------------ */

ListItr ListPushHead(List* _list, void* _item) {
    if (!_list) return NULL;
    Node* n = NodeCreate(_item);
    if (!n) return NULL;
    /* "Before the first real node" == "right after the sentinel". */
    NodeLinkBefore(_list->sentinel.next, n);
    return n;
}

ListItr ListPushTail(List* _list, void* _item) {
    if (!_list) return NULL;
    Node* n = NodeCreate(_item);
    if (!n) return NULL;
    /* "Before the sentinel" == "at the very end". */
    NodeLinkBefore(&_list->sentinel, n);
    return n;
}

void* ListPopHead(List* _list) {
    if (!_list) return NULL;
    Node* first = _list->sentinel.next;
    if (first == &_list->sentinel) return NULL;  /* empty */

    void* data = first->data;
    NodeUnlink(first);
    free(first);
    return data;
}

void* ListPopTail(List* _list) {
    if (!_list) return NULL;
    Node* last = _list->sentinel.prev;
    if (last == &_list->sentinel) return NULL;   /* empty */

    void* data = last->data;
    NodeUnlink(last);
    free(last);
    return data;
}

/* ------------------------------------------------------------------
 * Iterators
 * ------------------------------------------------------------------ */

ListItr ListItrBegin(const List* _list) { /*const List* _list- i wont modify list through this pointer*/
    if (!_list) return NULL;
    /* Cast away const: iterators are mutating handles by API. */
    return (ListItr)((List*)_list)->sentinel.next; /*(List*)_list - cast to produce non-const List* iterator */
} /*sentinel is a Node (a value, not a pointer), so we use . to access its next */
/*(ListItr) this cast isnt neccessar since it is a void* */


ListItr ListItrEnd(const List* _list) { 
    if (!_list) return NULL;
    return (ListItr)&((List*)_list)->sentinel; 
}

ListItr ListItrNext(ListItr _itr) {
    if (!_itr) return NULL;
    Node* n = (Node*)_itr;
    /* If already at end, stay at end. */
    if (n->is_sentinel) return _itr;
    return (ListItr)n->next;
}

ListItr ListItrPrev(ListItr _itr) {
    if (!_itr) return NULL;
    Node* n = (Node*)_itr;
    /* Begin's prev is the sentinel; per the spec, return begin instead. */
    if (n->prev->is_sentinel) return _itr;
    return (ListItr)n->prev;
}

void* ListItrGet(ListItr _itr) {
    if (!_itr) return NULL;
    Node* n = (Node*)_itr;
    if (n->is_sentinel) return NULL;   /* end iterator has no data */
    return n->data;
}

void* ListItrSet(ListItr _itr, void* _element) {
    if (!_itr) return NULL;
    Node* n = (Node*)_itr;
    if (n->is_sentinel) return NULL;
    void* old = n->data;
    n->data = _element;
    return old;
}

ListItr ListItrInsertBefore(ListItr _itr, void* _element) {
    if (!_itr) return NULL;
    Node* pos = (Node*)_itr;
    Node* n = NodeCreate(_element);
    if (!n) return NULL;
    NodeLinkBefore(pos, n);   /* works even when pos is the sentinel (= insert at tail) */
    return n;
}

void* ListItrRemove(ListItr _itr) {
    if (!_itr) return NULL;
    Node* n = (Node*)_itr;
    if (n->is_sentinel) return NULL;   /* cannot remove the sentinel */

    void* data = n->data;
    NodeUnlink(n);
    free(n);
    return data;
}

/* ------------------------------------------------------------------
 * Size / emptiness
 * ------------------------------------------------------------------ */

size_t ListSize(const List* _list) {
    if (!_list) return 0;
    size_t count = 0;
    const Node* sent = &_list->sentinel;
    const Node* cur  = sent->next;
    while (cur != sent) {
        ++count;
        cur = cur->next;
    }
    return count;
}

size_t ListIsEmpty(List* _list) {
    if (!_list) return 1;   /* a NULL list is "empty" */
    return _list->sentinel.next == &_list->sentinel; /*empty if sentinel points to itself*/
}

/* ------------------------------------------------------------------
 * ForEach
 * ------------------------------------------------------------------ */

ListItr ListItrForEach(ListItr _begin, ListItr _end,
                       ListActionFunction _action, void* _context) {
    if (!_begin || !_end || !_action) return _begin;

    Node* cur = (Node*)_begin;
    Node* end = (Node*)_end;

    while (cur != end) {
        if (!_action(cur->data, _context)) { /*iteration will stop if Action function returns 0 for an element*/
            return (ListItr)cur;   /* stopped here */
        }
        cur = cur->next;
    }
    return (ListItr)end;
}
