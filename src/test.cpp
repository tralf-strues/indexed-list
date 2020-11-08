#include <stdio.h>
#include <stdlib.h>
#include "../libs/log_generator.h"

// #define LIST_POISON            
// #define IS_LIST_POISON(value)  isnan(value)
typedef double list_elem_t;
#include "list.h"

int main()
{
    List list = {};
    constructList(&list, 5);

    pushFront(&list, -10);
    pushFront(&list, 20);
    pushFront(&list, 31);
    pushFront(&list, 4);
    pushFront(&list, 3);
    pushFront(&list, 90);
    pushFront(&list, 101);
    insertAfter(&list, 100, 1);
    insertAfter(&list, 99, 1);
    insertAfter(&list, 97, 1);
    insertBefore(&list, 90, 1);
    insertBefore(&list, 90, 3);

    // insertAfter(&list, 100, 1);
    // insertAfter(&list, 99, 1);
    // insertAfter(&list, 97, 1);
    // insertBefore(&list, 90, 1);
    // insertBefore(&list, 90, 3);

    // insertAfter(&list, 100, 1);
    // insertAfter(&list, 99, 1);
    // insertAfter(&list, 97, 1);
    // insertBefore(&list, 90, 1);
    // insertBefore(&list, 90, 3);

    // insertAfter(&list, 100, 1);
    // insertAfter(&list, 99, 1);
    // insertAfter(&list, 97, 1);
    // insertBefore(&list, 90, 1);
    // insertBefore(&list, 90, 3);

    // remove(&list, 2);
    // remove(&list, 1);

    // clear(&list);

    // LIST_SLOW::switchToIndexSearch(&list);

    // for (size_t i = 1; i < 10; i++)
    // {
    //     printf("findIndex(%lu) = %d\n", i, LIST_SLOW::findIndex(&list, i));
    //     printf("findPos  (%lu) = %d\n", i, LIST_SLOW::findPos(&list, i));
    // }

    dump(&list);
    LG_Close();

    destructList(&list);

    return 0;
}