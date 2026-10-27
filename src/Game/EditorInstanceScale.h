#ifndef HW_EDITOR_INSTANCE_SCALE_H
#define HW_EDITOR_INSTANCE_SCALE_H
#include <stdint.h>
#include <string.h>
#include <math.h>
#define EDITOR_SCALE_SLOTS 4096u
typedef struct EditorInstanceScaleEntry
{
    const void *owner;
    unsigned int identity;
    float scale;
    unsigned char state;
} EditorInstanceScaleEntry;
typedef struct EditorInstanceScaleStore
{
    EditorInstanceScaleEntry entries[EDITOR_SCALE_SLOTS];
    unsigned int count;
} EditorInstanceScaleStore;
static unsigned int editorScaleHash(const void *owner, unsigned int identity)
{
    uintptr_t address=(uintptr_t)owner;
    return (unsigned int)((address>>4)^(address>>16)^((uintptr_t)identity*2654435761u))&(EDITOR_SCALE_SLOTS-1u);
}
static float editorInstanceScaleGet(const EditorInstanceScaleStore *store,const void *owner,unsigned int identity)
{
    unsigned int i,slot;
    if(!owner || !store->count)return 1.0f;
    slot=editorScaleHash(owner,identity);
    for(i=0;i<EDITOR_SCALE_SLOTS;++i,slot=(slot+1)&(EDITOR_SCALE_SLOTS-1))
    {
        const EditorInstanceScaleEntry *entry=&store->entries[slot];
        if(!entry->state)return 1.0f;
        if(entry->state==1 && entry->owner==owner && entry->identity==identity)return entry->scale;
    }
    return 1.0f;
}
static int editorInstanceScaleSet(EditorInstanceScaleStore *store,const void *owner,unsigned int identity,float scale)
{
    unsigned int i,slot,empty=EDITOR_SCALE_SLOTS;
    if(!owner || !isfinite(scale) || scale<.05f || scale>50.0f)return 0;
    slot=editorScaleHash(owner,identity);
    for(i=0;i<EDITOR_SCALE_SLOTS;++i,slot=(slot+1)&(EDITOR_SCALE_SLOTS-1))
    {
        EditorInstanceScaleEntry *entry=&store->entries[slot];
        if(entry->state==1 && entry->owner==owner && entry->identity==identity)
        {
            if(scale==1.0f){entry->state=2;--store->count;}else entry->scale=scale;
            return 1;
        }
        if(entry->state!=1 && empty==EDITOR_SCALE_SLOTS)empty=slot;
        if(!entry->state)break;
    }
    if(scale==1.0f)return 1;
    if(empty==EDITOR_SCALE_SLOTS)return 0;
    store->entries[empty].owner=owner;store->entries[empty].identity=identity;
    store->entries[empty].scale=scale;store->entries[empty].state=1;++store->count;
    return 1;
}
static void editorInstanceScaleReset(EditorInstanceScaleStore *store)
{
    memset(store,0,sizeof(*store));
}
#endif
