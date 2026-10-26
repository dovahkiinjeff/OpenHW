#ifndef OPENHW_SHIP_AUTHORING_H
#define OPENHW_SHIP_AUTHORING_H
#include <stddef.h>
#define SHIP_AUTHOR_POINTS 512
#define SHIP_AUTHOR_MATERIALS 32
enum {SAP_JOINT,SAP_NAV,SAP_DOCK,SAP_SALVAGE,SAP_REPAIR,SAP_WEAPON,SAP_TURRET,SAP_ENGINE,SAP_SUBSYSTEM};
typedef struct ShipAuthorPoint {
 char name[64],parent[64];int kind,enabled,object,clone;
 float position[3],rotation[3],color[3],size;
 char subsystem[64];
 int subsystemRole; /* 0: legacy/unspecified, 1: weapon, 2: engine, 3: utility */
} ShipAuthorPoint;
typedef struct ShipAuthorMaterial {char name[64],texture[3][128];} ShipAuthorMaterial;
typedef struct ShipAuthorDocument {
 char name[48],model[128];int race,type;float scale;
 unsigned pointCount,materialCount;
 ShipAuthorPoint points[SHIP_AUTHOR_POINTS];ShipAuthorMaterial materials[SHIP_AUTHOR_MATERIALS];
} ShipAuthorDocument;
int shipAuthorNameValid(const char *name);
int shipAuthorDocumentValid(const ShipAuthorDocument *doc);
int shipAuthorProjectPath(const char *name,const char *relative,char *path,size_t bytes,int create);
int shipAuthorSave(const ShipAuthorDocument *doc);
int shipAuthorLoad(const char *name,ShipAuthorDocument *out);
int shipAuthorImportFile(const char *name,const char *source,const char *relative);
int shipAuthorCopyAssets(const ShipAuthorDocument *doc,const char *oldName);
int shipAuthorCopySelection(const ShipAuthorDocument *doc,unsigned selected,ShipAuthorDocument *clipboard);
int shipAuthorPaste(ShipAuthorDocument *doc,const ShipAuthorDocument *clipboard,float offset,int *selected);
int shipAuthorParent(const ShipAuthorDocument *doc,unsigned index);
int shipAuthorCategory(const ShipAuthorPoint *point);
void shipAuthorWorld(const ShipAuthorDocument *doc,unsigned index,float matrix[16]);
int shipAuthorReparent(ShipAuthorDocument *doc,unsigned index,int parent);
int shipAuthorRotateLocal(ShipAuthorDocument *doc,unsigned index,int axis,float degrees);
#endif
