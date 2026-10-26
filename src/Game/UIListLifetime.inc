void uicListCleanUp(listwindowhandle listwindow)
{
    /* A launch/hold refresh rebuilds these rows. Retire every row pointer
       BEFORE freeing them: SetCurItem otherwise clears flags through the old
       selection, which may already have been reused for unrelated game data. */
    listwindow->CurLineSelected = NULL;
    listwindow->topitem = NULL;
    listwindow->message = 0;
    listDeleteAll(&listwindow->listofitems);
    listwindow->ListTotal = 0;
    listwindow->UpperIndex = 0;

}

void uicListWindowSetCurItem(listwindowhandle listhandle, listitemhandle item)
{
    if (listhandle->CurLineSelected!=NULL)
    {
        bitClear(listhandle->CurLineSelected->flags, UICLI_Selected);
    }

    listhandle->CurLineSelected = item;
    if (listhandle->CurLineSelected!= NULL)
    {
        bitSet(listhandle->CurLineSelected->flags, UICLI_Selected);
        uicListWindowViewSelection(listhandle);
    }
}

