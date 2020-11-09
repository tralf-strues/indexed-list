#include <stdio.h>
#include <stdlib.h>
#include "../libs/log_generator.h"

#include "list.h"

void test1Empty(List* list)
{

}

void test2Pushing(List* list)
{
    pushFront(list, -10);
    pushFront(list, 20);
    pushFront(list, 31);
    pushFront(list, 4);
    pushFront(list, 3);
    pushFront(list, 90);
    pushFront(list, 101);

    pushBack(list, -10);
    pushBack(list, 20);
    pushBack(list, 31);
    pushBack(list, 4);
    pushBack(list, 3);
    pushBack(list, 90);
    pushBack(list, 101);
}

void test3Inserting(List* list)
{
    for (size_t i = 1; i <= 16; i++)
    {
        pushBack(list, i);
    }

    int idxMid = LIST_SLOW::findIndex(list, 8);

    for (size_t i = 0; i < 4; i++)
    {
        insertBefore(list, 0, idxMid);
        insertAfter(list, 1, idxMid);
    }
}

void test4Removing(List* list)
{
    test2Pushing(list);

    remove(list, 2);
    popBack(list);
    popFront(list);
}

void test5Clear(List* list)
{
    test2Pushing(list);

    clear(list);
}

void test6Sorting(List* list)
{
    test2Pushing(list);

    LIST_SLOW::switchToIndexSearch(list);
}

int main()
{
    List list = {};
    constructList(&list, 5);

    // test1Empty(&list);
    // test2Pushing(&list);
    // test3Inserting(&list);
    // test4Removing(&list);
    // test5Clear(&list);
    test6Sorting(&list);

    dump(&list);
    LG_Close();

    destructList(&list);

    return 0;
}