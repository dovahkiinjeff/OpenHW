#ifndef OPENHW_CATACLYSM_NEEDED_FLAGS_H
#define OPENHW_CATACLYSM_NEEDED_FLAGS_H
#include <stdint.h>
/* The existing save stream is delimiter-terminated. Appending hull IDs must
   not turn that delimiter (or the following chunk) into a hull's load flags. */
static __inline int hwCataReadNeededFlags(unsigned capacity,uint32_t delimiter,
    uint32_t (*readFlag)(void *),void (*applyFlag)(void *,unsigned,uint32_t),void *context)
{
    unsigned i;uint32_t flag;
    for(i=0;i<capacity;i++){
        flag=readFlag(context);if(flag==delimiter)return (int)i;
        applyFlag(context,i,flag);
    }
    return readFlag(context)==delimiter?(int)capacity:-1;
}
#endif
