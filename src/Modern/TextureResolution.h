#ifndef HW_TEXTURE_RESOLUTION_H
#define HW_TEXTURE_RESOLUTION_H
/* Long-edge caps; source mode never enlarges or discards the authored top mip. */
static unsigned int hwTextureResolutionCap(int quality)
{
    static const unsigned int caps[4] = {512, 1024, 2048, 0};
    return caps[quality >= 0 && quality <= 3 ? quality : 3];
}
static void hwTextureResolutionDimensions(unsigned int *width,
    unsigned int *height, unsigned int cap)
{
    unsigned int edge = *width > *height ? *width : *height;
    if (cap && edge > cap)
    {
        *width = (unsigned int)((unsigned long long)*width * cap / edge);
        *height = (unsigned int)((unsigned long long)*height * cap / edge);
        if (!*width) *width = 1;
        if (!*height) *height = 1;
    }
}
#endif
