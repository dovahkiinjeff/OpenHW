#ifndef HW_FORMATION_SPREAD_POLICY_H
#define HW_FORMATION_SPREAD_POLICY_H
#include <stdint.h>
#include <math.h>

#define FORMATION_SPREAD_DEFAULT 1000u
#define FORMATION_SPREAD_MAX 2000u
#define FORMATION_SORT_MASK UINT32_C(0x000000ff)
#define FORMATION_SPREAD_MAGIC UINT32_C(0xa0000000)

/* Extend the existing serialized 32-bit sort word without changing a command
   or save layout: sort byte, 11 slider bits, 9 tight-limit bits, 4-bit tag.
   Legacy words (0/1) and the centered slider retain exact original behavior. */
static inline unsigned formationSpreadTicks(uint32_t word)
{
    unsigned ticks = (word >> 8) & 2047u;
    unsigned tight = (word >> 19) & 511u;
    return (word & UINT32_C(0xf0000000)) == FORMATION_SPREAD_MAGIC &&
        ticks <= FORMATION_SPREAD_MAX && tight != 0 ? ticks : FORMATION_SPREAD_DEFAULT;
}
static inline float formationSpreadTight(uint32_t word)
{
    return (float)((word >> 19) & 511u) / 511.0f;
}
static inline uint32_t formationSpreadPack(uint32_t sort, unsigned ticks, float tight)
{
    unsigned quantized;
    if (ticks >= FORMATION_SPREAD_MAX) ticks = FORMATION_SPREAD_MAX;
    if (ticks == FORMATION_SPREAD_DEFAULT) return sort & FORMATION_SORT_MASK;
    if (!isfinite(tight) || tight > 1.0f) tight = 1.0f;
    if (tight < 0.05f) tight = 0.05f;
    /* Round outward so quantization cannot shrink below the clearance limit. */
    quantized = (unsigned)ceilf(tight * 511.0f - 0.0001f);
    if (quantized > 511u) quantized = 511u;
    return (sort & FORMATION_SORT_MASK) | FORMATION_SPREAD_MAGIC |
        (ticks << 8) | (quantized << 19);
}
static inline float formationSpreadScale(unsigned ticks, float tight)
{
    if (ticks > FORMATION_SPREAD_MAX) ticks = FORMATION_SPREAD_MAX;
    return ticks <= FORMATION_SPREAD_DEFAULT ?
        tight + (1.0f - tight) * ((float)ticks / 1000.0f) : (float)ticks / 1000.0f;
}
static inline float formationSpreadStoredScale(uint32_t word)
{
    unsigned ticks = formationSpreadTicks(word);
    return ticks == FORMATION_SPREAD_DEFAULT ? 1.0f : formationSpreadScale(ticks,formationSpreadTight(word));
}

typedef struct FormationSpreadBody
{
    float offset[3], center[3], half[3];
} FormationSpreadBody;

/* Fixed-orientation hull boxes need separation on at least one axis. Keep the
   whole authored formation shape and choose the smallest uniform scale which
   leaves a small clearance for EVERY pair, including different hull sizes. */
static inline float formationSpreadMinimum(const FormationSpreadBody *bodies, int count)
{
    int i,j,k;
    float minimum = 0.05f;
    for (i=0;i<count;++i) for (j=0;j<i;++j)
    {
        float pair = 1.0e30f;
        for (k=0;k<3;++k)
        {
            float d = bodies[i].offset[k] - bodies[j].offset[k];
            float c = bodies[i].center[k] - bodies[j].center[k];
            float radius = bodies[i].half[k] + bodies[j].half[k];
            float gap = radius * 0.02f, required;
            if (gap < 1.0f) gap = 1.0f;
            if (!isfinite(d) || !isfinite(c) || !isfinite(radius)) return 1.0f;
            if (fabsf(d)<0.0001f) continue;
            required = (radius + gap - (d > 0 ? c : -c)) / fabsf(d);
            if (required < pair) pair = required;
        }
        if (pair > minimum) minimum = pair;
    }
    /* Never expand a pre-existing overlapping/custom layout on a tight drag. */
    return minimum > 1.0f ? 1.0f : minimum;
}
#endif
