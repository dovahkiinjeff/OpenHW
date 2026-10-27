/* Optional loading artwork must never enter the fatal, required-file loader.
   In particular, an empty selection resolves to the install directory. */
static ubyte *hrReadOptionalArtwork(const char *name, int *length)
{
    filehandle handle;
    ubyte *bytes = NULL;
    int used = 0, capacity = 0;
    udword flags = FF_ReturnNULLOnFail;
    const int limit = 64 * 1024 * 1024;
    *length = 0;
    if (name == NULL || name[0] == '\0') return NULL;
    if (name[0] == '/' || name[0] == '\\' || name[1] == ':')
        flags |= FF_IgnorePrepend | FF_IgnoreBIG;
    handle = fileOpen((char *)name, flags);
    if (!handle) return NULL;
    while (used < limit)
    {
        int count;
        if (used == capacity)
        {
            int next = capacity ? capacity * 2 : 65536;
            ubyte *grown = (ubyte *)realloc(bytes, next);
            if (grown == NULL) { free(bytes); fileClose(handle); return NULL; }
            bytes = grown;
            capacity = next;
        }
        count = fileBlockReadNoError(handle, bytes + used, capacity - used);
        if (count <= 0) break;
        used += count;
    }
    fileClose(handle);
    if (used == 0 || used == limit) { free(bytes); return NULL; }
    *length = used;
    return bytes;
}
