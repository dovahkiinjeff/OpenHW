/* Exact production cleanup/selection functions; deterministic allocator reuse
   makes a stale write visible instead of relying on a random in-game crash. */
#include "UIControls.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks,views;
static uiclistwindow *observed;
static uiclistitem *recycled;
static int retiredBeforeFree;
static const udword reusedWord=0xffffffffu;
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}} while(0)

void listDeleteAll(LinkedList *list)
{
    Node *walk=list->head;
    retiredBeforeFree=observed->CurLineSelected==NULL && observed->topitem==NULL;
    while(walk)
    {
        uiclistitem *row=(uiclistitem *)listGetStructOfNode(walk);
        walk=walk->next;
        /* Model a freed row immediately reused by a different owner. The
           previous selection code MUST NOT clear bits in this new payload. */
        row->flags=reusedWord;
        recycled=row;
    }
    list->head=list->tail=NULL;list->num=0;
}
void uicListWindowViewSelection(listwindowhandle list)
{
    CHECK(list->CurLineSelected!=NULL);
    views++;
}
#ifndef UI_LIST_LIFETIME_SOURCE
#define UI_LIST_LIFETIME_SOURCE "../src/Game/UIListLifetime.inc"
#endif
#include UI_LIST_LIFETIME_SOURCE

static void refresh(int remaining)
{
    uiclistwindow list={0};
    uiclistitem old={0},replacement={0};
    int oldviews=views;
    observed=&list;recycled=NULL;
    old.link.structptr=&old;old.flags=UICLI_Selected|UICLI_CanSelect;
    list.listofitems.head=list.listofitems.tail=&old.link;list.listofitems.num=1;
    list.CurLineSelected=list.topitem=&old;list.message=CM_NewItemSelected;
    list.ListTotal=1;list.UpperIndex=7;
    /* The actual launch path saves the type, cleans the old rows, adds the
       remaining types, and restores a selection (NULL for the last launch). */
    uicListCleanUp(&list);
    CHECK(recycled==&old);
    if(remaining)
    {
        replacement.flags=UICLI_CanSelect;
        replacement.link.structptr=&replacement;
        list.listofitems.head=list.listofitems.tail=&replacement.link;
        list.listofitems.num=1;list.topitem=&replacement;list.ListTotal=1;
    }
    uicListWindowSetCurItem(&list,remaining ? &replacement : NULL);
    CHECK(old.flags==reusedWord); /* Old production code corrupts this word. */
    CHECK(retiredBeforeFree);
    CHECK(list.CurLineSelected==(remaining ? &replacement : NULL));
    CHECK(list.message==0 && list.UpperIndex==0);
    CHECK(views==oldviews+(remaining ? 1 : 0));
    if(remaining)
    {
        CHECK(replacement.flags&UICLI_Selected);
        uicListWindowSetCurItem(&list,NULL);
        CHECK(!(replacement.flags&UICLI_Selected));
    }
    uicListCleanUp(&list);
    uicListCleanUp(&list);
    CHECK(list.CurLineSelected==NULL && list.topitem==NULL && list.ListTotal==0);
}

int main(void)
{
    int iteration;
    for(iteration=0;iteration<1000;++iteration)
    {
        refresh(1); /* Some ships of the selected type remain in the bay. */
        refresh(0); /* Last ship of the selected type has just launched. */
    }
    printf("PASS: launch-list refresh never touches retired/reused rows (%d checks)\n",checks);
    return 0;
}
