#ifndef HW_NIS_FADE_POLICY_H
#define HW_NIS_FADE_POLICY_H
/* Advance by the actual per-tick fade amount; clamp at the target rather than
   snapping early for slow fades or overshooting for fast fades. */
static float hwNisFadeAdvance(float current, float target, float rate, float dt)
{
    float step = rate * dt;
    if (current == target) return target;
    if (current < target) return step <= 0.0f || step >= target-current ? target : current+step;
    return step >= 0.0f || -step >= current-target ? target : current+step;
}
#endif
