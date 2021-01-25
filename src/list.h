#include <stdint.h>
#include <math.h>

typedef double list_elem_t;

#ifdef LIST_DEBUG_MODE

#define ASSERT_LIST_OK(list) if(list == NULL || !listOk(list)) { dump(list); LG_Close(); assert(! "OK"); }
#define LIST_POISON            nan("")
#define IS_LIST_POISON(value)  isnan(value)
#define LIST_CANARIES_ENABLED

#else
#define ASSERT_LIST_OK(list) 
#endif

#if defined(LIST_POISON) && defined(IS_LIST_POISON)
#define LIST_POISONING_ENABLED
#endif

#ifdef LIST_CANARIES_ENABLED
static uint32_t LIST_ARRAY_CANARY_L = 0xBADC0FFE;
static uint32_t LIST_ARRAY_CANARY_R = 0xDEADBEEF;
#endif

static size_t      LIST_DEFAULT_CAPACITY    = 16;
static const char* LIST_GRAPH_TXT_FILE_NAME = "list_dump.txt";
static const char* LIST_GRAPH_IMG_FILE_NAME = "list_dump.svg";
static const char* LIST_LOG_FOLDER          = "log/";

#ifdef LIST_DEBUG_MODE
static const char* LIST_DYNAMICALLY_CREATED_NAME = "no name, created dynamically";
#else
static const char* LIST_DEFAULT_NOT_DEBUG_NAME   = "naming disabled (LIST_DEBUG_MODE undefined)";
#endif

enum ListError
{
    LIST_NO_ERROR            = 0x000, 
    LIST_POP_FROM_EMPTY      = 0x001,
    LIST_TOP_FROM_EMPTY      = 0x002,
    LIST_CONSTRUCTION_FAILED = 0x004,
    LIST_REALLOCATION_FAILED = 0x008,
    LIST_MEMORY_CORRUPTION   = 0x010,
    LIST_FREE_LIST_LOOP      = 0x020,
    LIST_LOOP                = 0x040,
    LIST_ACCESSING_ZERO      = 0x080

    #ifdef LIST_DEBUG_MODE
    ,
    LIST_NOT_CONSTRUCTED_USE = 0x100,
    LIST_DESTRUCTED_USE      = 0x200
    #endif
};

#ifdef LIST_DEBUG_MODE
enum ListStatus
{
    LIST_STATUS_NOT_CONSTRUCTED,
    LIST_STATUS_CONSTRUCTED,
    LIST_STATUS_DESTRUCTED
};
#endif

struct ListNode;

struct List
{
    #ifdef LIST_DEBUG_MODE
    const char* name = NULL;
    #endif

    ListNode*  nodes         = NULL;
    size_t     size          = 0;
    size_t     capacity      = 0;

    size_t     head          = 0;
    size_t     tail          = 0;
    size_t     free          = 0;
    bool       searchEnabled = true;
    uint32_t   errorStatus   = 0;

    #ifdef LIST_DEBUG_MODE
    ListStatus status = LIST_STATUS_NOT_CONSTRUCTED;
    #endif
};

#ifdef LIST_DEBUG_MODE

    #define constructList(list, bufferSize) fconstructList(list, bufferSize, &#list[1])
    #define constructDefaultList(list)      fconstructList(list, LIST_DEFAULT_CAPACITY, &#list[1])
    
    List*   fconstructList (List* list, size_t bufferSize, const char* listName);
    List*   fconstructList (List* list, const char* listName);

#else

    #define constructList(list, bufferSize) fconstructList(list, bufferSize)
    #define constructDefaultList(list)      fconstructList(list, LIST_DEFAULT_CAPACITY)
    
    List*   fconstructList (List* list, size_t bufferSize);
    List*   fconstructList (List* list);

#endif

void        destructList   (List* list);

List*       newList        (size_t bufferSize);
List*       newList        ();
void        deleteList     (List* list);

size_t      getSize        (List* list);
size_t      getCapacity    (List* list);
bool        isEmpty        (List* list);
uint32_t    getErrorStatus (List* list);
const char* getErrorStr    (ListError error);
bool        listNodesLoop  (List* list);

int         insertAfter    (List* list, list_elem_t value, size_t idx);
int         insertBefore   (List* list, list_elem_t value, size_t idx);
list_elem_t at             (List* list, size_t idx);
list_elem_t remove         (List* list, size_t idx);
void        clear          (List* list);

int         pushBack       (List* list, list_elem_t value);
int         pushFront      (List* list, list_elem_t value);
list_elem_t popBack        (List* list);
list_elem_t popFront       (List* list);
list_elem_t topBack        (List* list);
list_elem_t topFront       (List* list);

bool        find           (List* list, list_elem_t value, int* idx, int* pos);

bool        listOk         (List* list);
void        dump           (List* list);

namespace LIST_SLOW
{

void switchToIndexSearch (List* list);
int  findIndex           (List* list, size_t pos);
int  findPos             (List* list, size_t idx);

}