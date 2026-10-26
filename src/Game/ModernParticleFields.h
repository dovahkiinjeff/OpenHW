#ifndef MODERN_PARTICLE_FIELDS_H
#define MODERN_PARTICLE_FIELDS_H
#include "ModernGraphics.h"
#include <stdio.h>
void modernParticleFieldsReset(void);
void modernParticleFieldsUpdate(void);
void modernParticleFieldsDirty(void);
void modernParticleFieldsResetStyles(void);
int modernParticleFieldsCount(void);
int modernParticleFieldsOriginalVisible(void);
void modernParticleFieldsSetOriginalVisible(int visible);
int modernParticleFieldsOriginalColor(float rgb[3]);
void modernParticleFieldsSetOriginalColor(const float rgb[3]);
void modernParticleFieldsResetOriginalColor(void);
float modernParticleFieldsOriginalOpacity(void);
void modernParticleFieldsSetOriginalOpacity(float opacity);
int modernOriginalRopeColor(float rgb[3]);
void modernOriginalRopeSetColor(const float rgb[3]);
void modernOriginalRopeResetColor(void);
float modernOriginalRopeOpacity(void);
void modernOriginalRopeSetOpacity(float opacity);
void modernOriginalRopeReset(void);
HWModernHarvestableNebulaStyle *modernParticleFieldsStyle(int field, int edit);
void modernParticleFieldsResetOverride(int field);
void modernParticleFieldsWrite(FILE *stream);
int modernParticleFieldsRead(const char *line);
#endif
