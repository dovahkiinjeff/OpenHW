#include "ShipAuthorText.h"
#include <stdlib.h>
#include <string.h>
static char* normalize(const char*s,size_t n){size_t i,j=0;char*p;if(n>SHIP_TEXT_LIMIT)return NULL;p=malloc(n+1);if(!p)return NULL;for(i=0;i<n;i++){unsigned char c=s[i];if(!c||(c<32&&c!='\t'&&c!='\r'&&c!='\n')){free(p);return NULL;}if(c=='\r'){p[j++]='\n';if(i+1<n&&s[i+1]=='\n')i++;}else p[j++]=c;}p[j]=0;return p;}
void shipTextFree(ShipAuthorText*e){int i;free(e->text);for(i=0;i<e->count;i++)free(e->undo[i]);memset(e,0,sizeof(*e));}
int shipTextLoad(ShipAuthorText*e,const char*s,size_t n){char*p=normalize(s,n);if(!p)return 0;shipTextFree(e);e->text=p;return 1;}
int shipTextInsert(ShipAuthorText*e,const char*s,size_t n){size_t a=e->anchor<e->cursor?e->anchor:e->cursor,b=e->anchor>e->cursor?e->anchor:e->cursor,len=e->text?strlen(e->text):0,add;char*insert=normalize(s,n),*next;int i;if(!insert)return 0;add=strlen(insert);if(b>len||len-(b-a)+add>SHIP_TEXT_LIMIT){free(insert);return 0;}next=malloc(len-(b-a)+add+1);if(!next){free(insert);return 0;}memcpy(next,e->text,a);memcpy(next+a,insert,add);memcpy(next+a+add,e->text+b,len-b+1);free(insert);
 for(i=e->at;i<e->count;i++)free(e->undo[i]);e->count=e->at;if(e->count==32){free(e->undo[0]);memmove(e->undo,e->undo+1,31*sizeof(char*));e->count--;}
 e->undo[e->count++]=e->text;e->at=e->count;e->text=next;e->cursor=e->anchor=a+add;return 1;}
int shipTextUndo(ShipAuthorText*e,int redo){char*swap;int index=redo?e->at:e->at-1;if(index<0||index>=e->count)return 0;swap=e->text;e->text=e->undo[index];e->undo[index]=swap;e->at+=redo?1:-1;e->cursor=e->anchor=strlen(e->text);return 1;}
size_t shipTextLineStart(const ShipAuthorText*e,size_t at){while(at&&e->text[at-1]!='\n')at--;return at;}
size_t shipTextLineEnd(const ShipAuthorText*e,size_t at){while(e->text[at]&&e->text[at]!='\n')at++;return at;}
