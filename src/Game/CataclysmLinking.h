#ifndef OPENHW_CATACLYSM_LINKING_H
#define OPENHW_CATACLYSM_LINKING_H
#include "FEFlow.h"
void mrLinkAcolytes(char *name,featom *atom);
void mrUnlinkACVs(char *name,featom *atom);
void mrUpdateAcolyteTransitions(void);
void mrCancelAcolyteTransitions(void);
void mrSaveAcolyteTransitions(void);
void mrLoadAcolyteTransitions(void);
struct Ship;
bool32 mrAcolyteIsTransitioning(struct Ship *ship);
void univReallyRemoveShipForTransformation(struct Ship *ship);
#endif
