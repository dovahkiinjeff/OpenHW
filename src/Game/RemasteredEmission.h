#ifndef HW_REMASTERED_EMISSION_H
#define HW_REMASTERED_EMISSION_H
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
