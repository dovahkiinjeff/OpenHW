#ifndef OPENHW_ENGINE_FADE_H
#define OPENHW_ENGINE_FADE_H
/* Simulation-time interpolation: repeated render passes cannot advance it. */
static float engineFadeAdvance(float value, float elapsed, int active)
{
    const float duration = 0.75f;
    float step = elapsed > 0 ? elapsed / duration : 0;
    value += active ? step : -step;
    return value < 0 ? 0 : value > 1 ? 1 : value;
}
#endif
