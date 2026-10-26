#ifndef SHIP_AUTHOR_TEXT_H
#define SHIP_AUTHOR_TEXT_H
#include <stddef.h>
#define SHIP_TEXT_LIMIT (1024*1024)
typedef struct ShipAuthorText {char *text,*undo[32];size_t cursor,anchor;int count,at;} ShipAuthorText;
void shipTextFree(ShipAuthorText *e);
int shipTextLoad(ShipAuthorText *e,const char *text,size_t length);
int shipTextInsert(ShipAuthorText *e,const char *text,size_t length);
int shipTextUndo(ShipAuthorText *e,int redo);
size_t shipTextLineStart(const ShipAuthorText *e,size_t at);
size_t shipTextLineEnd(const ShipAuthorText *e,size_t at);
#endif
