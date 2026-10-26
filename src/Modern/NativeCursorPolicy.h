#ifndef HW_NATIVE_CURSOR_POLICY_H
#define HW_NATIVE_CURSOR_POLICY_H
#include <math.h>
/* Alpha-aware resizing avoids dark fringes around transparent cursor artwork.
   Coordinates are pixel centers; the cursor hotspot remains the original 0,0. */
static void hwCursorResizeRgba(const unsigned char *source, int sw, int sh,
    unsigned char *dest, int dw, int dh)
{
    int x, y, c;
    for (y = 0; y < dh; ++y) for (x = 0; x < dw; ++x)
    {
        float sx = ((float)x + 0.5f) * sw / dw - 0.5f;
        float sy = ((float)y + 0.5f) * sh / dh - 0.5f;
        int x0, y0, x1, y1, i;
        float fx, fy, weights[4], alpha = 0;
        const unsigned char *p[4];
        if (sx < 0) sx = 0;
        if (sy < 0) sy = 0;
        x0 = (int)sx; y0 = (int)sy;
        x1 = x0 + 1 < sw ? x0 + 1 : x0;
        y1 = y0 + 1 < sh ? y0 + 1 : y0;
        fx = sx - x0; fy = sy - y0;
        weights[0] = (1-fx)*(1-fy); weights[1] = fx*(1-fy);
        weights[2] = (1-fx)*fy; weights[3] = fx*fy;
        p[0] = source + (y0*sw+x0)*4; p[1] = source + (y0*sw+x1)*4;
        p[2] = source + (y1*sw+x0)*4; p[3] = source + (y1*sw+x1)*4;
        for (i = 0; i < 4; ++i) alpha += weights[i]*p[i][3];
        for (c = 0; c < 3; ++c)
        {
            float value = 0;
            for (i = 0; i < 4; ++i) value += weights[i]*p[i][3]*p[i][c];
            dest[(y*dw+x)*4+c] = alpha > 0 ? (unsigned char)(value/alpha+0.5f) : 0;
        }
        dest[(y*dw+x)*4+3] = (unsigned char)(alpha+0.5f);
    }
}
#endif
