#ifndef __NEW_GEN_DLIST_H__
#define __NEW_GEN_DLIST_H__

#include <stddef.h>  /* size_t */

typedef struct List List;
typedef void* ListItr;

/** * @brief Action function to operate on an element
 * @param _element : element to test
 * @param _context : context to be used
 * @return zero if action fails
 */
typedef int (*ListActionFunction)(void* _element, void* _context);

/** * @brief Create a list
 * @returns a pointer to the created list.
 * @retval NULL on failure due to allocation failure
 */
List* ListCreate(void);

/** * @brief Destroy list
 * @params[in] _pList : Pointer to List; on completion *_pList will be null
 * @params[in] _elementDestroy : Function pointer to destroy items, or NULL
 */
void ListDestroy(List** _pList, void (*_elementDestroy)(void* _item));

/** * @brief Add element to head of list. Time complexity: O(1).
 * @return an iterator pointing at the element inserted or NULL on error
 */
ListItr ListPushHead(List* _list, void* _item);

/** * @brief Add element to the list's tail. Time complexity O(1).
 * @return an iterator pointing at the element inserted or NULL on error
 */
ListItr ListPushTail(List* _list, void* _item);

/** @brief Remove element from list's head. Time complexity O(1).
 * @return the data that was removed or NULL on error
 */
void* ListPopHead(List* _list);

/** @brief Remove element from list's tail. Time complexity O(1).
 * @return the data that was removed or NULL on error
 */
void* ListPopTail(List* _list);

/** * @brief Get iterator to the list's beginning (first real element)
 */
ListItr ListItrBegin(const List* _list);

/** * @brief Get iterator to the list end (the Tail sentinel)
 */
ListItr ListItrEnd(const List* _list);

/** * @brief Get iterator to the next element
 */
ListItr ListItrNext(ListItr _itr);

/** * @brief Get iterator to the previous element
 */
ListItr ListItrPrev(ListItr _itr);

/** * @brief Get data from the list node the iterator is pointing to
 */
void* ListItrGet(ListItr _itr);

/** * @brief Set data at the node where the iterator is pointing at
 * @return the old data from the node
 */
void* ListItrSet(ListItr _itr, void* _element);

/** * @brief Inserts a new node before the node the iterator is pointing at
 * @return an iterator pointing at the element inserted or NULL on error
 */
ListItr ListItrInsertBefore(ListItr _itr, void* _element);

/** * @brief Removes the node the iterator is pointing at
 * @return the removed data
 */
void* ListItrRemove(ListItr _itr);

/** @brief Get number of elements in the list. Average time O(n).
 */
size_t ListSize(const List* _list);

/** @brief Check if a given list is empty
 */
size_t ListIsEmpty(List* _list);

/** * @brief execute an action for all elements in a half-open range [begin..end)
 * @return ListItr to the element where the iteration stopped
 */
ListItr ListItrForEach(ListItr _begin, ListItr _end, ListActionFunction _action, void* _context);

#endif /* __NEW_GEN_DLIST_H__ */