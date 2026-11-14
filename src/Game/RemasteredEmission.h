#ifndef HW_REMASTERED_EMISSION_H
#define HW_REMASTERED_EMISSION_H
/* Uniform authored masks are losslessly stored as 1x1 DDS pages. A black
   constant is an explicit no-emission map, not a missing page. */
static int meshRemasteredEmissionDimensionsValid(unsigned sw,unsigned sh,unsigned w,unsigned h)
{return w>0&&h>0&&((sw==w&&sh==h)||(sw==1&&sh==1));}
static const unsigned char *meshRemasteredEmissionTexel(const unsigned char *rgba,
    unsigned sw,unsigned sh,unsigned index)
{return rgba+((sw==1&&sh==1)?0:index)*4;}
/* Authored emission alpha is not necessarily an emission mask: RM sidecars
   historically copied opaque hull alpha, including black non-glowing texels. */
static unsigned char meshRemasteredEmissionCoverage(const unsigned char rgba[4],
                                                    unsigned char hullCoverage)
{
    unsigned int peak = rgba[0];
    unsigned int coverage = rgba[3] < hullCoverage ? rgba[3] : hullCoverage;
    if (rgba[1] > peak) peak = rgba[1];
    if (rgba[2] > peak) peak = rgba[2];
    /* This alpha controls hybrid lighting protection, not emitted RGB energy.
       A faint authored glow must not exempt an entire opaque hull texel. */
    return (unsigned char)((peak * coverage + 127u) / 255u);
}
#endif
