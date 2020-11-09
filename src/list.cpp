#include <assert.h>
#include <stdlib.h>
#include "list.h"
#include "../libs/log_generator.h"

const unsigned char LIST_MAX_ERRORS_COUNT  = 20;
const unsigned char LIST_MAX_DOT_CMD_SIZE  = 64;
const double        LIST_EXPAND_MULTIPLIER = 1.8;
const size_t        LIST_MINIMAL_CAPACITY  = 4;

struct ListNode
{
    list_elem_t value = 0;
    int         prev  = -1;
    int         next  = -1;
};

bool      listFreeLoop    (List* list);
int       getLastFree     (List* list);
void      listUpdateFree  (List* list, size_t begin);
ListNode* resize          (List* list, size_t newCapacity);
void      setError        (List* list, ListError error);
void      dumpPrintErrors (List* list, const char* indentation);
void      dumpGraph       (List* list);

//-----------------------------------------------------------------------------
//! Updates free list, sets values to LIST_POISON if LIST_POISONING_ENABLED
//! is defined and sets prev values to -1.
//!
//! @param [out] list  
//! @param [in]  begin     
//-----------------------------------------------------------------------------
void listUpdateFree(List* list, size_t begin)
{
    assert(list != NULL);
    assert(begin > list->size);

    if (list->free == 0) { list->free = begin; }

    for (size_t i = begin; i < list->capacity; i++)
    {
        #ifdef LIST_POISONING_ENABLED
        list->nodes[i].value = LIST_POISON;
        #endif

        list->nodes[i].prev = -1;

        if (list->free == begin && i == list->capacity - 1)
            list->nodes[i].next = 0;
        else if (i == list->capacity - 1)
            list->nodes[i].next = list->free;
        else
            list->nodes[i].next = i + 1;
    }
}

#ifdef LIST_POISONING_ENABLED
#define LIST_CHECK_POISON(list) listCheckPoison(list)

//-----------------------------------------------------------------------------
//! @param [out] list   
//!
//! @warning Sets list's errorStatus to LIST_MEMORY_CORRUPTION if there's an 
//!          unused element that doesn't have POISON or a used element that
//!          has it.
//!
//! @return whether or not list's nodes buffer has LIST_POISON in unused space
//!         and doesn't have LIST_POISON in used space.
//-----------------------------------------------------------------------------
bool listCheckPoison(List* list)
{
    assert(list != NULL);

    int valueIterator = list->head;
    for (size_t i = 0; valueIterator != 0; i++)
    {
        if (i > list->size - 1)
        {
            setError(list, LIST_LOOP);
            return false;
        }

        if (IS_LIST_POISON(list->nodes[valueIterator].value))
        {
            setError(list, LIST_MEMORY_CORRUPTION);
            return false;
        }

        valueIterator = list->nodes[valueIterator].next;
    }

    size_t maxFreeCount = list->capacity - list->size - 1;
    int    freeIterator = list->free;

    for (size_t i = 0; freeIterator != 0; i++)
    {
        if (i > maxFreeCount - 1)
        {
            setError(list, LIST_FREE_LIST_LOOP);
            return false;
        }

        if (!IS_LIST_POISON(list->nodes[freeIterator].value))
        {
            setError(list, LIST_MEMORY_CORRUPTION);
            return false;
        }

        freeIterator = list->nodes[freeIterator].next;
    }

    return true;
}

#else
#define LIST_CHECK_POISON(list) true
#endif

#ifdef LIST_CANARIES_ENABLED
    #define GET_CANARY(memBlock, memBlockSize, side)               getCanary  (memBlock, memBlockSize, side)
    #define SET_CANARY(memBlock, memBlockSize, canary, side)       setCanary  (memBlock, memBlockSize, canary,  side)
    #define SET_CANARIES(memBlock, memBlockSize, canaryL, canaryR) setCanaries(memBlock, memBlockSize, canaryL, canaryR)
    #define LIST_CHECK_CANARIES(list)                              listCheckCanaries(list)
    #define LIST_SET_CANARIES(list)                                SET_CANARIES((void*)list->nodes,                \
                                                                                list->capacity * sizeof(ListNode), \
                                                                                LIST_ARRAY_CANARY_L,               \
                                                                                LIST_ARRAY_CANARY_R);              

//-----------------------------------------------------------------------------
//! Returns left (side = 'l') or right (side = 'r') canary of the memBlock. 
//!
//! @param [in] memBlock  
//! @param [in] memBlockSize   
//! @param [in] side indicates which canary to return - 'l' for
//!                  left and 'r' for right
//!
//! @warning Undefined behavior in case there's no canary protection for 
//!          memBlock.
//!
//! @return canary value or 0 if side isn't 'l' or 'r'
//-----------------------------------------------------------------------------
uint32_t getCanary(void* memBlock, size_t memBlockSize, char side)
{
    if (side == 'l')
    {
        return *(uint32_t*)((char*) memBlock - sizeof(uint32_t));
    }
    else if (side == 'r')
    {
        return *(uint32_t*)((char*) memBlock + memBlockSize);
    }

    return 0;
}

//-----------------------------------------------------------------------------
//! Sets left (side = 'l') or right (side = 'r') canary of the memBlock. 
//! Does nothing if side isn't 'l' or 'r'.
//!
//! @param [out] memBlock  
//! @param [in]  memBlockSize   
//! @param [in]  canary   
//! @param [in]  side indicates which canary to return - 'l' for
//!                   left and 'r' for right
//!
//! @warning Undefined behavior in case there's no canary protection for 
//!          memBlock. 
//-----------------------------------------------------------------------------
void setCanary(void* memBlock, size_t memBlockSize, uint32_t canary, char side)
{
    if (side == 'l')
    {
        *(uint32_t*)((char*) memBlock - sizeof(uint32_t)) = canary;
    }
    else if (side == 'r')
    {
        *(uint32_t*)((char*) memBlock + memBlockSize) = canary;
    }
}

//-----------------------------------------------------------------------------
//! Sets left and right canaries of the memBlock.
//!
//! @param [out] memBlock  
//! @param [in]  memBlockSize   
//! @param [in]  canaryL   
//! @param [in]  canaryR   
//!
//! @warning Undefined behavior in case there's no canary protection for 
//!          memBlock.
//-----------------------------------------------------------------------------
void setCanaries(void* memBlock, size_t memBlockSize, uint32_t canaryL, uint32_t canaryR)
{
    setCanary(memBlock, memBlockSize, canaryL, 'l');
    setCanary(memBlock, memBlockSize, canaryR, 'r');
}

//-----------------------------------------------------------------------------
//! Checks whether or not list's canaries have correct values. 
//!
//! @param [out] list  
//!
//! @warning Sets list's errorStatus to LIST_MEMORY_CORRUPTION if canaries are
//!          incorrect.  
//!
//! @return whether or not list's canaries have correct values.
//-----------------------------------------------------------------------------
bool listCheckCanaries(List* list)
{
    if (getCanary((void*)list->nodes, list->capacity * sizeof(ListNode), 'l') != LIST_ARRAY_CANARY_L || 
        getCanary((void*)list->nodes, list->capacity * sizeof(ListNode), 'r') != LIST_ARRAY_CANARY_R)
    {
        setError(list, LIST_MEMORY_CORRUPTION);
        return false;
    }

    return true;
}

#else
    #define GET_CANARY(memBlock, memBlockSize, side)               
    #define SET_CANARY(memBlock, memBlockSize, canary, side)       
    #define SET_CANARIES(memBlock, memBlockSize, canaryL, canaryR) 
    #define LIST_CHECK_CANARIES(list) true
    #define LIST_SET_CANARIES(list)
#endif

//-----------------------------------------------------------------------------
//! List's constructor. Allocates max(capacity, LIST_MINIMAL_CAPACITY) 
//! objects of type ListNode.
//!
//! @param [out] list  
//! @param [in]  capacity   
//!
//! @note if calloc returned NULL then sets list's errorStatus to 
//!       LIST_INITIALIZATION_FAILED.
//!
//! @return list if constructed successfully or NULL otherwise.
//-----------------------------------------------------------------------------
#ifdef LIST_DEBUG_MODE
List* fconstructList(List* list, size_t capacity, const char* listName)
#else
List* fconstructList(List* list, size_t capacity)
#endif
{
    assert(list != NULL);
    assert(capacity > 0);

    #ifdef LIST_DEBUG_MODE
    list->name = listName;
    #endif

    list->size     = 0;
    list->capacity = capacity + 1 > LIST_MINIMAL_CAPACITY ? capacity + 1 : LIST_MINIMAL_CAPACITY;

    list->nodes = (ListNode*) ((char*)calloc(1, list->capacity * sizeof(ListNode) 

                                                   #ifdef LIST_CANARIES_ENABLED
                                                   + sizeof(LIST_ARRAY_CANARY_L) 
                                                   + sizeof(LIST_ARRAY_CANARY_R) 
                                                   #endif 
                                            )

                                            #ifdef LIST_CANARIES_ENABLED
                                            + sizeof(LIST_ARRAY_CANARY_L)
                                            #endif 
                              );

    if (list->nodes == NULL) 
    {
        setError(list, LIST_CONSTRUCTION_FAILED);
        ASSERT_LIST_OK(list);
        return NULL;
    }

    LIST_SET_CANARIES(list);    

    list->head          = 0;
    list->tail          = 0;
    list->free          = 0;
    list->searchEnabled = true;

    #ifdef LIST_POISONING_ENABLED
    list->nodes[0].value = LIST_POISON;
    #endif

    list->nodes[0].next  = 0;
    list->nodes[0].prev  = 0;

    #ifdef LIST_DEBUG_MODE
    list->status = LIST_STATUS_CONSTRUCTED;
    #endif

    listUpdateFree(list, 1);

    ASSERT_LIST_OK(list);

    return list;
}

//-----------------------------------------------------------------------------
//! List's constructor. Allocates LIST_DEFAULT_CAPACITY objects of type 
//! ListNode.
//!
//! @param [out] list   
//!
//! @note if calloc returned NULL then sets list's errorStatus to 
//!       LIST_INITIALIZATION_FAILED.
//!
//! @return list if constructed successfully or NULL otherwise.
//-----------------------------------------------------------------------------
#ifdef LIST_DEBUG_MODE
List* fconstructList(List* list, const char* listName)
#else
List* fconstructList(List* list)
#endif
{
    #ifdef LIST_DEBUG_MODE
    list = fconstructList(list, LIST_DEFAULT_CAPACITY, listName);
    #else
    list = fconstructList(list, LIST_DEFAULT_CAPACITY);
    #endif

    ASSERT_LIST_OK(list);

    return list;
}

//-----------------------------------------------------------------------------
//! List's destructor. Calls free for list's nodes.
//!
//! @param [out] list   
//-----------------------------------------------------------------------------
void destructList(List* list)
{
    ASSERT_LIST_OK(list);

    #ifdef LIST_CANARIES_ENABLED
    free((char*)list->nodes - sizeof(LIST_ARRAY_CANARY_L));
    #else
    free(list->nodes);
    #endif

    list->size     = 0;
    list->capacity = 0;
    list->nodes    = NULL;

    #ifdef LIST_DEBUG_MODE
    list->status   = LIST_STATUS_DESTRUCTED;
    #endif
}

//-----------------------------------------------------------------------------
//! Allocates a List, calls constructor and returns the pointer to this list.
//!
//! @param [in] capacity   
//!
//! @note if calloc returned NULL then sets list's errorStatus to 
//!       LIST_INITIALIZATION_FAILED.
//! @note if capacity is less than LIST_MINIMAL_CAPACITY, than sets capacity
//!       to LIST_MINIMAL_CAPACITY.
//!
//! @return list if constructed successfully or NULL otherwise.
//-----------------------------------------------------------------------------
List* newList(size_t capacity)
{
    assert(capacity > 0);

    List* newList = (List*) calloc(1, sizeof(List));

    #ifdef LIST_DEBUG_MODE
    fconstructList(newList, LIST_DYNAMICALLY_CREATED_NAME);
    #else
    fconstructList(newList);
    #endif

    return newList;
}

//-----------------------------------------------------------------------------
//! Allocates a List, calls constructor and returns the pointer to this list.
//!
//! @note if calloc returned NULL then sets list's errorStatus to 
//!       LIST_INITIALIZATION_FAILED.
//! @note starting capacity is LIST_DEFAULT_CAPACITY
//!
//! @return list if constructed successfully or NULL otherwise.
//-----------------------------------------------------------------------------
List* newList()
{
    return newList(LIST_DEFAULT_CAPACITY);
}

//-----------------------------------------------------------------------------
//! Calls destructor of list and free of list. Undefined behavior if list
//! wasn't created dynamically using newList() or an allocator.
//!
//! @param [out] list   
//-----------------------------------------------------------------------------
void deleteList(List* list)
{
    ASSERT_LIST_OK(list);

    destructList(list);

    free(list);
}

//-----------------------------------------------------------------------------
//! @param [in] list   
//!
//! @return current size of list.
//-----------------------------------------------------------------------------
size_t getSize(List* list)
{
    ASSERT_LIST_OK(list);

    return list->size;
}

//-----------------------------------------------------------------------------
//! @param [in] list   
//!
//! @return current capacity of list.
//-----------------------------------------------------------------------------
size_t getCapacity(List* list)
{
    ASSERT_LIST_OK(list);

    return list->capacity;
}

//-----------------------------------------------------------------------------
//! @param [in] list   
//!
//! @return whether or not list is empty.
//-----------------------------------------------------------------------------
bool isEmpty(List* list)
{
    ASSERT_LIST_OK(list);

    return list->size == 0;
}

//-----------------------------------------------------------------------------
//! @param [in] list   
//!
//! @return current errorStatus of list.
//-----------------------------------------------------------------------------
uint32_t getErrorStatus(List* list)
{
    assert(list != NULL);

    return list->errorStatus;
}

//-----------------------------------------------------------------------------
//! Adds error to list's errorStatus.
//!
//! @param [out] list   
//! @param [in]  error   
//-----------------------------------------------------------------------------
void setError(List* list, ListError error)
{
    assert(list != NULL);

    list->errorStatus |= error;
}

//----------------------------------------------------------------------------- 
//! @param [in] error   
//!    
//! @return string representation of error (e.g. error = LIST_POP_FROM_EMPTY
//!         returns "LIST_POP_FROM_EMPTY").   
//-----------------------------------------------------------------------------
const char* getErrorStr(ListError error)
{
    #define TO_STR(value) #value

    switch (error)
    {
        case LIST_NO_ERROR:            return TO_STR(LIST_NO_ERROR);
        case LIST_POP_FROM_EMPTY:      return TO_STR(LIST_POP_FROM_EMPTY);
        case LIST_TOP_FROM_EMPTY:      return TO_STR(LIST_TOP_FROM_EMPTY);
        case LIST_CONSTRUCTION_FAILED: return TO_STR(LIST_CONSTRUCTION_FAILED);
        case LIST_REALLOCATION_FAILED: return TO_STR(LIST_REALLOCATION_FAILED);
        case LIST_MEMORY_CORRUPTION:   return TO_STR(LIST_MEMORY_CORRUPTION);
        case LIST_FREE_LIST_LOOP:      return TO_STR(LIST_FREE_LIST_LOOP);
        case LIST_LOOP:                return TO_STR(LIST_LOOP);
        case LIST_ACCESSING_ZERO:      return TO_STR(LIST_ACCESSING_ZERO);

        #ifdef LIST_DEBUG_MODE
        case LIST_NOT_CONSTRUCTED_USE: return TO_STR(LIST_NOT_CONSTRUCTED_USE);
        case LIST_DESTRUCTED_USE:      return TO_STR(LIST_DESTRUCTED_USE);
        #endif

        default: return NULL;
    }

    #undef TO_STR
}

//-----------------------------------------------------------------------------
//! @param [in] list  
//! 
//! @return whether or not there are loops in list.
//-----------------------------------------------------------------------------
bool listNodesLoop(List* list)
{
    int currNode = list->head;
    for (size_t i = 0; list->nodes[currNode].next != 0; i++)
    {
        if (i >= list->size - 1)
        {
            return true;
        }

        currNode = list->nodes[currNode].next;
    }

    return false;
}

//-----------------------------------------------------------------------------
//! @param [in] list  
//! 
//! @return whether or not there are loops the free list of list.
//-----------------------------------------------------------------------------
bool listFreeLoop(List* list)
{
    assert(list        != NULL);
    assert(list->nodes != NULL);

    size_t maxFreeCount = list->capacity - list->size - 1;
    if (maxFreeCount == 0)
    {
        return false;
    }

    int lastFree = list->free;
    for (size_t i = 0; list->nodes[lastFree].next != 0; i++)
    {
        if (i >= maxFreeCount - 1)
        {
            return true;
        }

        lastFree = list->nodes[lastFree].next;
    }

    return false;
}

//-----------------------------------------------------------------------------
//! Resizes list's array to newCapacity. 
//!
//! @param [out]  list   
//! @param [in]   newCapacity   
//!
//! @warning If newCapacity is less than list's size then there will be DATA 
//!          LOSS!
//!
//! @warning If reallocation was unsuccessful, returns NULL and sets list's 
//!          errorStatus to LIST_REALLOCATION_FAILED, but current elements of 
//!          the list won't be removed.
//!
//! @return pointer to the new list's array if reallocation was successful 
//!         or NULL otherwise.
//-----------------------------------------------------------------------------
ListNode* resize(List* list, size_t newCapacity)
{
    ASSERT_LIST_OK(list);
    ListNode* newArray = NULL;

    newArray = (ListNode*) ((char*) realloc((char*) list->nodes 
                                                 
                                            #ifdef LIST_CANARIES_ENABLED
                                            - sizeof(LIST_ARRAY_CANARY_L)
                                            #endif

                                            , 
                                            newCapacity * sizeof(ListNode) 
                                            #ifdef LIST_CANARIES_ENABLED
                                            + sizeof(LIST_ARRAY_CANARY_L) 
                                            + sizeof(LIST_ARRAY_CANARY_R)
                                            #endif
                                           ) 

                            #ifdef LIST_CANARIES_ENABLED
                            + sizeof(LIST_ARRAY_CANARY_L)
                            #endif  
                           );

    if (newArray == NULL)
    {
        setError(list, LIST_REALLOCATION_FAILED);
    }
    else
    {
        list->nodes    = newArray;
        list->capacity = newCapacity;

        LIST_SET_CANARIES(list);
        listUpdateFree(list, list->size + 1);
    }

    list->searchEnabled = true;

    ASSERT_LIST_OK(list);

    return newArray;
}

//-----------------------------------------------------------------------------
//! Inserts value to list after node with index idx (indexing starts from 1). 
//!
//! @param [out] list   
//! @param [in]  value   
//! @param [in]  idx   
//!
//! @note If idx = 0, then sets value as the first element in the list.
//! @note Can call resize function if there are no free space left.
//!
//! @return index at which value was inserted.
//-----------------------------------------------------------------------------
int insertAfter(List* list, list_elem_t value, size_t idx)
{
    ASSERT_LIST_OK(list);
    assert(idx < list->capacity);
    assert(list->nodes[idx].prev != -1);

    if (list->size == list->capacity - 1)
    {
        ListNode* newArray = resize(list, list->capacity * LIST_EXPAND_MULTIPLIER);

        if (newArray == NULL) { return 0; }
    }

    int newFree = list->nodes[list->free].next;

    if (list->size == 0)
    {
        list->nodes[list->free].prev = 0;
        list->nodes[list->free].next = 0;
        list->head = list->free;
        list->tail = list->free;
    }
    else if (idx == list->tail)
    {
        list->nodes[list->free].next = 0;
        list->nodes[list->free].prev = list->tail;
        list->nodes[list->tail].next = list->free;
        list->tail = list->free;
    }
    else if (idx == 0)
    {
        list->nodes[list->free].next = list->head;
        list->nodes[list->free].prev = 0;
        list->nodes[list->head].prev = list->free;
        list->head = list->free;
    }
    else
    {
        list->nodes[list->free].next            = list->nodes[idx].next;
        list->nodes[list->free].prev            = idx;
        list->nodes[list->nodes[idx].next].prev = list->free;
        list->nodes[idx].next                   = list->free;
    }

    int insertedIndex = list->free;

    list->nodes[list->free].value = value;
    list->free = newFree;
    list->size++;

    list->searchEnabled = true;

    ASSERT_LIST_OK(list);

    return insertedIndex;
}

//-----------------------------------------------------------------------------
//! Inserts value to list before node with index idx (indexing starts from 1). 
//!
//! @param [out] list   
//! @param [in]  value   
//! @param [in]  idx   
//!
//! @warning if idx = 0 then sets list's errorStatus to LIST_ACCESSING_ZERO and
//!          returns 0.
//! @note Can call resize function if there are no free space left.
//!
//! @return index at which value was inserted.
//-----------------------------------------------------------------------------
int insertBefore(List* list, list_elem_t value, size_t idx)
{
    ASSERT_LIST_OK(list);
    assert(list->nodes[idx].prev != -1);

    if (idx == 0)
    {
        setError(list, LIST_ACCESSING_ZERO);
        return 0;
    }

    return insertAfter(list, value, list->nodes[idx].prev);
}

//-----------------------------------------------------------------------------
//! Returns element at idx in list (indexing starts from 1). 
//!
//! @param [in] list   
//! @param [in] idx   
//!
//! @return element at idx.
//-----------------------------------------------------------------------------
list_elem_t at(List* list, size_t idx)
{
    ASSERT_LIST_OK(list);
    assert(idx > 0 && idx < list->capacity);
    assert(list->nodes[idx].prev != -1);

    return list->nodes[idx].value;
}

//-----------------------------------------------------------------------------
//! Removes element at idx from list (indexing starts from 1). 
//!
//! @param [out] list   
//! @param [in]  idx   
//!
//! @return element removed.
//-----------------------------------------------------------------------------
list_elem_t remove(List* list, size_t idx)
{
    ASSERT_LIST_OK(list);
    assert(idx > 0 && idx < list->capacity);
    assert(list->nodes[idx].prev != -1);

    list_elem_t value = list->nodes[idx].value;

    if (list->nodes[idx].prev != 0)
    {
        list->nodes[list->nodes[idx].prev].next = list->nodes[idx].next;
    }

    if (list->nodes[idx].next != 0)
    {
        list->nodes[list->nodes[idx].next].prev = list->nodes[idx].prev;
    }

    #ifdef LIST_POISONING_ENABLED
    list->nodes[idx].value = LIST_POISON;
    #endif

    if (idx == list->head)
    {
        list->head = list->nodes[idx].next;
    }

    if (idx == list->tail)
    {
        list->tail = list->nodes[idx].prev;
    }

    list->nodes[idx].prev = -1;
    list->nodes[idx].next = list->free;
    list->free            = idx;

    list->size--;

    list->searchEnabled = true;

    ASSERT_LIST_OK(list);

    return value;
}

//-----------------------------------------------------------------------------
//! Empties the list. 
//!
//! @param [out] list   
//-----------------------------------------------------------------------------
void clear(List* list)
{
    ASSERT_LIST_OK(list);

    list->head          = 0;
    list->tail          = 0;
    list->free          = 1;
    list->searchEnabled = true;
    list->size          = 0;

    #ifdef LIST_POISONING_ENABLED
    list->nodes[0].value = LIST_POISON;
    #endif

    list->nodes[0].next  = 0;
    list->nodes[0].prev  = 0;

    listUpdateFree(list, 1);

    ASSERT_LIST_OK(list);
}

//-----------------------------------------------------------------------------
//! Inserts value at the end of list. 
//!
//! @param [out] list   
//! @param [in]  value    
//!
//! @note Can call resize function if there are no free space left.
//!
//! @return index at which value was inserted.
//-----------------------------------------------------------------------------
int pushBack(List* list, list_elem_t value)
{
    ASSERT_LIST_OK(list);

    return insertAfter(list, value, list->tail);
}

//-----------------------------------------------------------------------------
//! Inserts value at the beginning of list. 
//!
//! @param [out] list   
//! @param [in]  value    
//!
//! @note Can call resize function if there are no free space left.
//!
//! @return index at which value was inserted.
//-----------------------------------------------------------------------------
int pushFront(List* list, list_elem_t value)
{
    ASSERT_LIST_OK(list);

    return insertAfter(list, value, 0);
}

//-----------------------------------------------------------------------------
//! Removes the last element from list. 
//!
//! @param [out] list   
//!
//! @return element removed.
//-----------------------------------------------------------------------------
list_elem_t popBack(List* list)
{
    ASSERT_LIST_OK(list);

    return remove(list, list->tail);
}

//-----------------------------------------------------------------------------
//! Removes the first element from list. 
//!
//! @param [out] list   
//!
//! @return element removed.
//-----------------------------------------------------------------------------
list_elem_t popFront(List* list)
{
    ASSERT_LIST_OK(list);

    return remove(list, list->head);
}

//-----------------------------------------------------------------------------
//! @param [out] list   
//!
//! @return the last element in list.
//-----------------------------------------------------------------------------
list_elem_t topBack(List* list)
{
    ASSERT_LIST_OK(list);

    return at(list, list->tail);
}

//-----------------------------------------------------------------------------
//! @param [out] list   
//!
//! @return the first element in list.
//-----------------------------------------------------------------------------
list_elem_t topFront(List* list)
{
    ASSERT_LIST_OK(list);

    return at(list, list->head);
}

//-----------------------------------------------------------------------------
//! Finds index and position of the first element with this value in list.
//! 
//! @param [in]  list   
//! @param [in]  value   
//! @param [out] idx   will be set to the index of the found element (starting 
//!              from 1) or 0 in case there no such elements in the list.    
//! @param [out] pos   will be set to the position of the found element (starting 
//!              from 1) or 0 in case there no such elements in the list.  
//!
//! @return whether or not element with this value has been found.
//-----------------------------------------------------------------------------
bool find(List* list, list_elem_t value, int* idx, int* pos)
{
    ASSERT_LIST_OK(list);

    int index = list->head;
    for (size_t i = 1; i <= list->size; i++)
    {
        if (list->nodes[index].value == value)
        {
            *idx = index;
            *pos = i;

            return true;
        }

        index = list->nodes[index].next;
    }

    *idx = 0;
    *pos = 0;

    return false;
}

namespace LIST_SLOW
{

//-----------------------------------------------------------------------------
//! Optimizes findIndex and findPos functions by sorting the buffer.
//! 
//! @param [out] list   
//!
//! @note Supposed to be used the following way: 
//!       1. A sequence of insert/remove calls
//!       2. switchToIndexSearch
//!       3. A sequence of findIndex/findPos calls
//-----------------------------------------------------------------------------
void switchToIndexSearch(List* list)
{
    ASSERT_LIST_OK(list);

    ListNode* newNodes = (ListNode*) ((char*)calloc(1, list->capacity * sizeof(ListNode) 

                                                       #ifdef LIST_CANARIES_ENABLED
                                                       + sizeof(LIST_ARRAY_CANARY_L) 
                                                       + sizeof(LIST_ARRAY_CANARY_R) 
                                                       #endif 

                                                    )

                                                    #ifdef LIST_CANARIES_ENABLED
                                                    + sizeof(LIST_ARRAY_CANARY_L)
                                                    #endif 
                                     );
    assert(newNodes != NULL);

    newNodes[0] = list->nodes[0];

    for (size_t i = 1; i <= list->size; i++)
    {
        newNodes[i] = list->nodes[list->head];

        newNodes[i].next = i < list->size ? i + 1 : 0;
        newNodes[i].prev = i > 1          ? i - 1 : 0;

        list->head = list->nodes[list->head].next;
    }

    #ifdef LIST_CANARIES_ENABLED
    free((char*)list->nodes - sizeof(LIST_ARRAY_CANARY_L));
    #else
    free(list->nodes);
    #endif

    list->nodes = newNodes;

    list->head = list->size > 0 ? 1 : 0;
    list->tail = list->size;

    list->free = 0;
    listUpdateFree(list, list->size + 1);

    LIST_SET_CANARIES(list);    

    list->searchEnabled = false;

    ASSERT_LIST_OK(list);
}

//-----------------------------------------------------------------------------
//! Finds index of the element in list's buffer with position pos in list.
//! 
//! @param [in] list   
//! @param [in] pos   
//!
//! @return found index.
//-----------------------------------------------------------------------------
int findIndex(List* list, size_t pos)
{
    assert(list != NULL);
    assert(list->nodes != NULL);
    assert(list->size >= pos);
    assert(pos >= 1);

    if (!list->searchEnabled) { return pos; }

    size_t index = list->head;
    for (size_t i = 1; i < pos; i++)
    {
        index = list->nodes[index].next;
    }

    return index;
}

//-----------------------------------------------------------------------------
//! Finds position of the element in list by its index idx in list's buffer.
//! 
//! @param [in] list   
//! @param [in] idx   
//!
//! @return found position.
//-----------------------------------------------------------------------------
int findPos(List* list, size_t idx)
{
    assert(list != NULL);
    assert(list->nodes != NULL);

    if (list->nodes[idx].prev == -1) { return 0; }

    if (!list->searchEnabled) { return idx; }

    int pos = 1;
    while (list->nodes[idx].prev != 0)
    {
        idx = list->nodes[idx].prev;
        pos++;
    }    

    return pos;
}
    
}

//-----------------------------------------------------------------------------
//! @param [out] list   
//!
//! @return whether or not list is working correctly.
//-----------------------------------------------------------------------------
bool listOk(List* list)
{
    if (list == NULL) { return false; }

    if (list->errorStatus != 0)
    {
        return false;
    }

    #ifdef LIST_DEBUG_MODE
    if (list->status == LIST_STATUS_NOT_CONSTRUCTED)
    {
        setError(list, LIST_NOT_CONSTRUCTED_USE);
        return false;
    }

    if (list->status == LIST_STATUS_DESTRUCTED)
    {
        setError(list, LIST_DESTRUCTED_USE);
        return false;
    }
    #endif

    if (list->size < 0 || list->size > list->capacity)
    {
        setError(list, LIST_MEMORY_CORRUPTION);
        return false;
    }

    if (list->nodes == NULL)
    {
        setError(list, LIST_MEMORY_CORRUPTION);
        return false;
    }

    if (listNodesLoop(list))
    {
        setError(list, LIST_LOOP);
        return false;
    }

    if (listFreeLoop(list))
    {
        setError(list, LIST_FREE_LIST_LOOP);
        return false;
    }

    if (!LIST_CHECK_POISON(list))
    {
        return false;
    }

    if (!LIST_CHECK_CANARIES(list))
    {
        return false;
    }

    return true;
}

void dumpPrintErrors(List* list, const char* indentation)
{
    assert(list != NULL);

    if (list->errorStatus == LIST_NO_ERROR)
    {
        LG_Write(getErrorStr(LIST_NO_ERROR), LG_STYLE_CLASS_GOOD);
    }

    unsigned char errorsCount = 0;
    uint32_t      currBit     = 0;
    for (unsigned char i = 0; i < LIST_MAX_ERRORS_COUNT; i++)
    {
        currBit = (list->errorStatus >> i) & 1;

        if (currBit == 1)
        {
            errorsCount++;
        }
    }

    unsigned char errorsPrinted = 0;
    for (unsigned char i = 0; i < LIST_MAX_ERRORS_COUNT; i++)
    {
        currBit = (list->errorStatus >> i) & 1;

        if (currBit == 1)
        {
            currBit = currBit << i;

            if (errorsPrinted > 0 && indentation != NULL)
            {
                LG_Write(",\n%s", indentation);
            } 
            else if (errorsPrinted > 0)
            {
                LG_Write(",");
            }

            LG_Write("ERROR 0x%X: %s", LG_STYLE_CLASS_ERROR, currBit, getErrorStr((ListError) currBit));

            if (errorsCount == 1) { return; }

            errorsPrinted++;
        }
    }
}

void dumpGraph(List* list)
{
    assert(list != NULL);

    char* tempBuffer = (char*) calloc(LIST_MAX_DOT_CMD_SIZE, sizeof(char));
    assert(tempBuffer != NULL);

    snprintf(tempBuffer, LIST_MAX_DOT_CMD_SIZE, "%s%s", LIST_LOG_FOLDER, LIST_GRAPH_TXT_FILE_NAME);

    FILE* graphFile = fopen(tempBuffer, "w");
    assert(graphFile != NULL);

    fprintf(graphFile,
            "digraph structs {\n"
            "\trankdir=TB;\n\n"
            "\tnode [shape=\"record\", style=\"filled\", color=\"#000000\", fillcolor=\"#90EE90\"];\n\n"
            "\t{ rank = same; HEAD; NULL; TAIL }\n\n"
            "\tHEAD [label=\"Head\", fontsize=16.0, fontcolor=\"#F0FFFF\", fillcolor=\"#7B68EE\"];\n"
            "\tNULL [label=\"NULL\", fontsize=16.0, fontcolor=\"#F0FFFF\", fillcolor=\"#DC143C\"];\n"
            "\tTAIL [label=\"Tail\", fontsize=16.0, fontcolor=\"#F0FFFF\", fillcolor=\"#7B68EE\"];\n\n"
            "\tHEAD->NODE1[style=invis];\n"
            "\tTAIL->NODE1[style=invis];\n\n");

    for (size_t i = 1; i < list->capacity; i++)
    {
        fprintf(graphFile, "\tNODE%u [label=\"{", i);

        // if not free node
        if (list->nodes[i].prev != -1)
        {
            fprintf(graphFile,
                    "[%u] %lg|{pos\\n%d|",
                    i,
                    list->nodes[i].value,
                    LIST_SLOW::findPos(list, i));
        }
        else
        {
            fprintf(graphFile, "[%u] FREE|{", i);
        }

        fprintf(graphFile, "<nxt>nxt\\n");

        if (list->nodes[i].next == 0) { fprintf(graphFile, "NULL"); }
        else                          { fprintf(graphFile, "%d", list->nodes[i].next); }

        fprintf(graphFile, "|<prv>prv\\n");

        if (list->nodes[i].prev == 0) { fprintf(graphFile, "NULL"); }
        else                          { fprintf(graphFile, "%d", list->nodes[i].prev); }

        // if not free node
        if (list->nodes[i].prev != -1) { fprintf(graphFile, "}}\"];\n"); }
        else                           { fprintf(graphFile, "}}\", fillcolor=\"#FFA07A\"];\n"); }

        if (i < list->capacity - 1) 
        { 
            fprintf(graphFile, "\tNODE%u->NODE%u[style=invis];\n", i, i + 1); 
        }
    }

    for (size_t i = 1; i < list->capacity; i++)
    {
        if (list->nodes[i].prev != -1 && list->nodes[list->nodes[i].next].prev != -1)
        {
            if (list->nodes[i].next != 0)
            {
                fprintf(graphFile, "\n\tNODE%u:<nxt> -> NODE%u [constraint=false];\n", 
                        i, list->nodes[i].next);
            }

            if (list->nodes[i].prev != 0)
            {
                fprintf(graphFile, "\n\tNODE%u:<prv> -> NODE%u [color=\"#A9A9A9\", style=\"dashed\", constraint=false];\n",
                        i, list->nodes[i].prev);
            }
        }
    }

    fprintf(graphFile, "\tHEAD -> ");
    if (list->head == 0) { fprintf(graphFile, "NULL"); }
    else                 { fprintf(graphFile, "NODE%d", list->head); }
    fprintf(graphFile, " [color=\"#7B68EE\"];\n");

    fprintf(graphFile, "\tTAIL -> ");
    if (list->tail == 0) { fprintf(graphFile, "NULL"); }
    else                 { fprintf(graphFile, "NODE%d", list->tail); }
    fprintf(graphFile, " [color=\"#7B68EE\"];\n}");

    fclose(graphFile);

    snprintf(tempBuffer, 
             LIST_MAX_DOT_CMD_SIZE,
             "dot -Tsvg %s%s > %s%s", 
             LIST_LOG_FOLDER, 
             LIST_GRAPH_TXT_FILE_NAME, 
             LIST_LOG_FOLDER, 
             LIST_GRAPH_IMG_FILE_NAME);

    printf("%s\n", tempBuffer);

    system(tempBuffer);

    free(tempBuffer);

    LG_AddImage(LIST_GRAPH_IMG_FILE_NAME, "max-width: 280px;");
}

//-----------------------------------------------------------------------------
//! Uses log_generator to dump list to html log file. 
//!
//! @param [in] list   
//-----------------------------------------------------------------------------
void dump(List* list)
{
    assert(list != NULL);

    if (!LG_IsInitialized())
    {
        LG_Init();
    }

    LG_WriteMessageStart(LG_COLOR_BLACK);
    LG_Write("List (");
    dumpPrintErrors(list, "      ");

    LG_Write(") [0x%X] \"%s\"\n"
             "{\n"  
             "    size          = %lu\n"
             "    capacity      = %lu\n"
             "    head          = %d\n"
             "    tail          = %d\n"
             "    free          = %d\n"
             "    searchEnabled = %d\n"
             "    nodes [0x%X]\n"
             "    {\n"
              
             #ifdef LIST_CANARIES_ENABLED
             "        canaryL: 0x%lX | must be 0x%lX\n"
             "        canaryR: 0x%lX | must be 0x%lX\n"
             #endif
             ,
              
             list, 
              
             #ifdef LIST_DEBUG_MODE
             list->name, 
             #else
             LIST_DEFAULT_NOT_DEBUG_NAME, 
             #endif
             
             list->size,
             list->capacity,
             list->head,
             list->tail,
             list->free,
             list->searchEnabled,
             list->nodes
              
             #ifdef LIST_CANARIES_ENABLED
             ,getCanary((void*)list->nodes, list->capacity * sizeof(ListNode), 'l'),
              LIST_ARRAY_CANARY_L,
              getCanary((void*)list->nodes, list->capacity * sizeof(ListNode), 'r'),
              LIST_ARRAY_CANARY_R
             #endif  
    );
           
    for (size_t i = 0; i < list->capacity; i++)
    {
        if (i == 0)
        {
            LG_Write("        ![%lu]\t= ", i);
        }
        else if (list->nodes[i].prev != -1)
        {
            LG_Write("        *[%lu]\t= ", i);
        }
        else
        {
            LG_Write("         [%lu]\t= ", i);
        }
        
        LG_Write("[value=%-7lg, next=%-4d, prev=%-4d]", list->nodes[i].value,
                                                        list->nodes[i].next,
                                                        list->nodes[i].prev);
         
        #ifdef LIST_POISONING_ENABLED
        if (IS_LIST_POISON(list->nodes[i].value))
        {
            LG_Write(" (POISON!)");
        }
        #endif
           
        LG_Write("\n");
    }
       
    LG_Write("    }\n");
     
    dumpGraph(list);
     
    LG_Write("\n}\n");
    

    LG_WriteMessageEnd();
}