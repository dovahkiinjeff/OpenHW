#ifndef HW_CLOUD_RENDER_SETTINGS_H
#define HW_CLOUD_RENDER_SETTINGS_H

#include "ModernGraphics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static inline HWModernCloudRenderSettings hwModernCloudRenderSettingsDefaults(void)
{
    HWModernCloudRenderSettings settings = {0.0f, 3u, 0u, 1.0f, 1.0f, 0.80f};
    return settings;
}

static inline float hwModernCloudRenderSettingClamp(float value, float minimum,
                                                   float maximum, float fallback)
{
    if (!isfinite(value)) return fallback;
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static inline HWModernCloudRenderSettings hwModernCloudRenderSettingsSanitize(
    const HWModernCloudRenderSettings *source)
{
    HWModernCloudRenderSettings settings = hwModernCloudRenderSettingsDefaults();
    if (!source) return settings;
    settings.samplingQuality = hwModernCloudRenderSettingClamp(
        source->samplingQuality, -5.0f, 10.0f, settings.samplingQuality);
    settings.detail = source->detail > 3u ? 3u : source->detail;
    settings.motionEnabled = source->motionEnabled ? 1u : 0u;
    settings.motionSpeed = hwModernCloudRenderSettingClamp(
        source->motionSpeed, 0.0f, 5.0f, settings.motionSpeed);
    settings.billowStrength = hwModernCloudRenderSettingClamp(
        source->billowStrength, 0.0f, 2.0f, settings.billowStrength);
    settings.fieldCoverage = hwModernCloudRenderSettingClamp(
        source->fieldCoverage, 0.0f, 1.0f, settings.fieldCoverage);
    return settings;
}

/* Unknown versions and malformed/non-finite records leave the caller's current
   settings untouched. Missing records in older maps use reset defaults. */
static inline int hwModernCloudRenderSettingsRead(
    const char *line, HWModernCloudRenderSettings *settings)
{
    char token[32];
    int version = 0;
    int end = 0;
    int count;
    HWModernCloudRenderSettings parsed = hwModernCloudRenderSettingsDefaults();
    if (!line || !settings || sscanf(line, "%31s %d", token, &version) != 2 ||
        strcmp(token, "CLOUD_RENDERER")) return 0;
    if (version == 1)
        count = sscanf(line, "%31s %d %f %u %u %f %f %n",
            token, &version, &parsed.samplingQuality, &parsed.detail,
            &parsed.motionEnabled, &parsed.motionSpeed, &parsed.billowStrength, &end);
    else if (version == 2)
        count = sscanf(line, "%31s %d %f %u %u %f %f %f %n",
            token, &version, &parsed.samplingQuality, &parsed.detail,
            &parsed.motionEnabled, &parsed.motionSpeed, &parsed.billowStrength,
            &parsed.fieldCoverage, &end);
    else return 0;
    if (count != (version == 1 ? 7 : 8) || !end || line[end] ||
        !isfinite(parsed.samplingQuality) || !isfinite(parsed.motionSpeed) ||
        !isfinite(parsed.billowStrength) || !isfinite(parsed.fieldCoverage)) return 0;
    *settings = hwModernCloudRenderSettingsSanitize(&parsed);
    return 1;
}

static inline int hwModernCloudRenderSettingsWrite(
    FILE *file, const HWModernCloudRenderSettings *source)
{
    HWModernCloudRenderSettings settings = hwModernCloudRenderSettingsSanitize(source);
    return file && fprintf(file, "CLOUD_RENDERER 2 %.9g %u %u %.9g %.9g %.9g\n",
        settings.samplingQuality, settings.detail, settings.motionEnabled,
        settings.motionSpeed, settings.billowStrength, settings.fieldCoverage) > 0;
}

#endif
