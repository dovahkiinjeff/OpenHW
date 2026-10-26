#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Memory.h"
#include "File.h"
#include "Vector.h"
#include "Mesh.h"
#include "CRC32.h"
#include "RemasteredSubsystem.h"
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
#include "PersistentMeshSource.h"
#endif

typedef struct meshRmCapture {
    HwRmSubsystemNode nodes[255];
    char names[255][512], textures[255][512];
    unsigned int count;
} meshRmCapture;

static void meshRmCaptureNode(void *user, const HwRmSubsystemNode *node)
{
    meshRmCapture *capture = (meshRmCapture *)user;
    if (capture->count < 255) capture->nodes[capture->count++] = *node;
}

meshdata *meshCreateRemasteredHierarchy(HwRmSubsystemInstance *instance,
    unsigned int lod, float scale, const char *fileName,
    meshRmNameResolver materialName, meshRmNameResolver bindingName, void *user)
{
    meshRmCapture *capture;
    meshdata *mesh = NULL;
    unsigned int i, j, materials = 0, materialIndex = 0;
    memsize bytes;
    ubyte *cursor;
    char error[256];
    polygonobject *rootTail = NULL;
    if (!instance || !fileName || !materialName || strlen(fileName) >= FL_Path) return NULL;
    capture = (meshRmCapture *)calloc(1, sizeof(*capture));
    if (!capture) return NULL;
    if (!hwRmSubsystemVisitHierarchy(instance, lod, scale, 255,
        meshRmCaptureNode, capture, error, sizeof(error)) || !capture->count ||
        capture->nodes[0].parent != -1) goto failed;
    bytes = sizeof(meshdata) + (capture->count - 1) * sizeof(polygonobject);
    for (i = 0; i < capture->count; ++i)
    {
        const HwRmSubsystemNode *node = &capture->nodes[i];
        const char *name = bindingName && !node->part ? bindingName(user, node) : NULL;
        if (node->index != i || node->parent >= (int)capture->count ||
            node->parent == (int)i) goto failed;
        if (node->part)
            snprintf(capture->names[i], sizeof(capture->names[i]), "_RMgeometry_%u", i);
        else if (name && *name)
        {
            if (strlen(name) >= sizeof(capture->names[i])) goto failed;
            strcpy(capture->names[i], name);
        }
        else if (node->component)
        {
            if (snprintf(capture->names[i], sizeof(capture->names[i]), "%s.%s", node->slot, node->jointName)
                >= sizeof(capture->names[i])) goto failed;
        }
        else
        {
            if (strlen(node->jointName) >= sizeof(capture->names[i])) goto failed;
            strcpy(capture->names[i], node->jointName);
        }
        bytes += strlen(capture->names[i]) + 1;
        if (!node->part) continue;
        if (!strcmp(node->material->shader,"default")) {
            unsigned k;int uniform=1,neutral=1;
            if (node->material->parameterCount || !node->part->vertexColors) goto failed;
            for(k=0;k<node->part->vertexCount;++k) {
                unsigned rgba=node->part->vertexColors[k];
                if ((rgba&255)!=255) goto failed;
                uniform &= rgba==node->part->vertexColors[0];
                neutral &= (rgba>>24)==((rgba>>16)&255) && (rgba>>24)==((rgba>>8)&255);
            }
            if (!uniform && !neutral) goto failed;
        }
        for (j = 0; j < node->part->vertexCount; ++j)
        {
            unsigned int axis;
            for (axis = 0; axis < 3; ++axis)
                if (!isfinite(node->part->vertices[j].position[axis] * scale)) goto failed;
        }
        name = materialName(user, node);
        if (!name || !*name || strlen(name) >= sizeof(capture->textures[i]) ||
            strlen(fileName) + strlen(name) + 8 >= FL_Path ||
            node->part->vertexCount > 65535 || node->part->indexCount % 3) goto failed;
        strcpy(capture->textures[i], name);
        bytes += sizeof(materialentry) + strlen(name) + 1 +
            node->part->vertexCount * sizeof(vertexentry) +
            (node->part->vertexCount + node->part->indexCount / 3) * sizeof(normalentry) +
            (node->part->indexCount / 3) * sizeof(polyentry);
        if (bytes > 256 * 1024 * 1024) goto failed;
        ++materials;
    }
    if (!materials) goto failed;
    mesh = memAlloc(bytes, "Native RM hierarchy", NonVolatile);
    memset(mesh, 0, bytes);
    mesh->localSize = UDWORD_Max;
    mesh->nPublicMaterials = materials;
    mesh->nPolygonObjects = capture->count;
#if MESH_RETAIN_FILENAMES
    mesh->fileName = memStringDupe((char *)fileName);
#endif
    cursor = (ubyte *)mesh + sizeof(meshdata) + (capture->count - 1) * sizeof(polygonobject);
    mesh->localMaterial = mesh->publicMaterial = (materialentry *)cursor;
    cursor += materials * sizeof(materialentry);
    /* Geometry buffers first, so string lengths cannot misalign numeric arrays. */
    for (i = 0; i < capture->count; ++i)
    {
        const HwRmSubsystemNode *node = &capture->nodes[i];
        polygonobject *obj = &mesh->object[i];
        obj->iObject = (ubyte)i; obj->flags = POF_UniqueObject;
        memcpy(&obj->localMatrix, node->localMatrix, 16 * sizeof(float));
        if (node->parent < 0)
        {
            if (rootTail) rootTail->pSister = obj;
            rootTail = obj;
        }
        else
        {
            polygonobject **tail = &mesh->object[node->parent].pDaughter;
            while (*tail) tail = &(*tail)->pSister;
            *tail = obj; obj->pMother = &mesh->object[node->parent];
        }
        if (!node->part) continue;
        obj->nVertices = obj->nVertexNormals = node->part->vertexCount;
        obj->nPolygons = obj->nFaceNormals = node->part->indexCount / 3;
        obj->pVertexList = (vertexentry *)cursor; cursor += obj->nVertices * sizeof(vertexentry);
        obj->pNormalList = (normalentry *)cursor; cursor += (obj->nVertices + obj->nPolygons) * sizeof(normalentry);
        obj->pPolygonList = (polyentry *)cursor; cursor += obj->nPolygons * sizeof(polyentry);
        for (j = 0; j < (unsigned int)obj->nVertices; ++j)
        {
            const HwHodVertex *src = &node->part->vertices[j];
            vertexentry *vertex = &obj->pVertexList[j];
            normalentry *normal = &obj->pNormalList[obj->nPolygons + j];
            vector n;
            vertex->x = src->position[1] * scale;
            vertex->y = -src->position[0] * scale;
            vertex->z = src->position[2] * scale;
            vertex->iVertexNormal = obj->nPolygons + j;
            n.x = src->normal[1]; n.y = -src->normal[0]; n.z = src->normal[2];
            if (vecMagnitudeSquared(n) > .000000001f) vecNormalize(&n);
            memcpy(&normal->x, &n, 3 * sizeof(float));
        }
        for (j = 0; j < (unsigned int)obj->nPolygons; ++j)
        {
            polyentry *poly = &obj->pPolygonList[j];
            const unsigned short *indices = &node->part->indices[j * 3];
            const HwHodVertex *a = &node->part->vertices[indices[0]],
                *b = &node->part->vertices[indices[1]], *c = &node->part->vertices[indices[2]];
            vector edge0, edge1, n;
            poly->iV0 = indices[0]; poly->iV1 = indices[1]; poly->iV2 = indices[2];
            poly->iFaceNormal = j; poly->iMaterial = (uword)materialIndex;
            poly->s0 = a->uv[0]; poly->t0 = a->uv[1];
            poly->s1 = b->uv[0]; poly->t1 = b->uv[1];
            poly->s2 = c->uv[0]; poly->t2 = c->uv[1];
            if (!strcmp(node->material->shader,"default"))
            {
                /* This verified source program is untextured vertex tint.
                   Its opaque neutral colors are represented by exact texel
                   centres in the staged 256x1 ramp, not fake emissive paint. */
                poly->s0=((node->part->vertexColors[indices[0]]>>24)+.5f)/256;
                poly->s1=((node->part->vertexColors[indices[1]]>>24)+.5f)/256;
                poly->s2=((node->part->vertexColors[indices[2]]>>24)+.5f)/256;
                poly->t0=poly->t1=poly->t2=.5f;
            }
            vecSub(edge0, (*(vector *)&obj->pVertexList[indices[1]].x), (*(vector *)&obj->pVertexList[indices[0]].x));
            vecSub(edge1, (*(vector *)&obj->pVertexList[indices[2]].x), (*(vector *)&obj->pVertexList[indices[0]].x));
            vecCrossProduct(n, edge0, edge1);
            if (vecMagnitudeSquared(n) > .000000001f) vecNormalize(&n);
            else n = *(vector *)&obj->pNormalList[obj->nPolygons + indices[0]].x;
            memcpy(&obj->pNormalList[j].x, &n, 3 * sizeof(float));
        }
        ++materialIndex;
    }
    materialIndex = 0;
    for (i = 0; i < capture->count; ++i)
    {
        polygonobject *obj = &mesh->object[i];
        obj->pName = (char *)cursor; strcpy(obj->pName, capture->names[i]); cursor += strlen(obj->pName) + 1;
        obj->nameCRC = crc16Compute((ubyte *)obj->pName, strlen(obj->pName));
        if (!capture->nodes[i].part) continue;
        {
            materialentry *material = &mesh->localMaterial[materialIndex++];
            material->pName = material->textureNameSave = (char *)cursor;
            strcpy((char *)cursor, capture->textures[i]); cursor += strlen((char *)cursor) + 1;
            material->texture = (trhandle *)material->textureNameSave;
            material->ambient = material->diffuse = colRGB(255,255,255);
            material->kAlpha = 1; material->flags = MDF_Smoothing; material->nFullAmbient = 64;
        }
    }
    mesh->name = mesh->object[0].pName;
#ifdef HW_ENABLE_D3D12_NATIVE_RASTER
    for(i=0;i<capture->count;++i) {
        polygonobject *obj=&mesh->object[i];HWPersistentMeshSource source={0};
        source.identity=obj->pPolygonList;source.vertices=obj->pVertexList;
        source.normals=obj->pNormalList;source.polygons=obj->pPolygonList;
        source.vertexCount=obj->nVertices;source.normalCount=obj->nFaceNormals+obj->nVertexNormals;
        source.polygonCount=obj->nPolygons;source.vertexStride=sizeof(vertexentry);
        source.normalStride=sizeof(normalentry);source.polygonStride=sizeof(polyentry);
        source.normalIndexOffset=offsetof(vertexentry,iVertexNormal);
        source.faceNormalOffset=offsetof(polyentry,iFaceNormal);source.materialOffset=offsetof(polyentry,iMaterial);
        source.indexOffset[0]=offsetof(polyentry,iV0);source.indexOffset[1]=offsetof(polyentry,iV1);source.indexOffset[2]=offsetof(polyentry,iV2);
        source.uvOffset[0]=offsetof(polyentry,s0);source.uvOffset[1]=offsetof(polyentry,s1);source.uvOffset[2]=offsetof(polyentry,s2);
        hwglRegisterImmutableMesh(&source);
    }
#endif
failed:
    free(capture);
    return mesh;
}

#define MESH_RM_POSE_BINDING_FLAG 0x40000000u

static bool32 meshRmKeepInstancePose(udword flags, hmatrix *startMatrix,
    hmatrix *matrixDest, void *user, smemsize id)
{
    (void)startMatrix; (void)matrixDest; (void)user; (void)id;
    /* The native state adapter wrote the per-ship binding matrix already. */
    /* Distinct from legacy pause/no-op callbacks, including optimized linker
       identical-code folding. The ownership flag is checked before reuse. */
    return (flags & MESH_RM_POSE_BINDING_FLAG) != 0;
}

static bool32 meshRmCollectTraversal(meshdata *mesh, polygonobject *object,
    unsigned int *indices, unsigned int *count, unsigned char *seen)
{
    for (; object; object = object->pSister)
    {
        unsigned int index;
        if (object < mesh->object || object >= mesh->object + mesh->nPolygonObjects) return FALSE;
        index = (unsigned int)(object - mesh->object);
        if (seen[index] || *count >= (unsigned int)mesh->nPolygonObjects) return FALSE;
        seen[index] = 1; indices[(*count)++] = index;
        if (!meshRmCollectTraversal(mesh, object->pDaughter, indices, count, seen)) return FALSE;
    }
    return TRUE;
}

bool32 meshApplyRemasteredHierarchyPose(HwRmSubsystemInstance *instance,
    unsigned int lod, float scale, meshdata *mesh, mhlocalbinding *bindings,
    unsigned int bindingCount)
{
    meshRmCapture *capture;
    unsigned int indices[255], count = 0, i;
    unsigned char seen[255] = {0};
    char error[256];
    if (!instance || !mesh || !bindings || bindingCount == 0 || bindingCount > 255 ||
        bindingCount != (unsigned int)mesh->nPolygonObjects) return FALSE;
    capture = (meshRmCapture *)calloc(1, sizeof(*capture));
    if (!capture) return FALSE;
    if (!hwRmSubsystemVisitHierarchy(instance, lod, scale, 255, meshRmCaptureNode,
        capture, error, sizeof(error)) || capture->count != bindingCount ||
        !meshRmCollectTraversal(mesh, mesh->object, indices, &count, seen) || count != bindingCount)
        goto failedPose;
    for (i = 0; i < count; ++i)
    {
        const unsigned int index = indices[i];
        const HwRmSubsystemNode *node = &capture->nodes[index];
        const polygonobject *object = &mesh->object[index];
        const int parent = object->pMother ? (int)(object->pMother - mesh->object) : -1;
        if (node->index != index || node->parent != parent ||
            (node->part ? (int)node->part->vertexCount : 0) != object->nVertices ||
            (node->part ? (int)node->part->indexCount / 3 : 0) != object->nPolygons ||
            (bindings[i].object && bindings[i].object != object) ||
            (bindings[i].function && (!(bindings[i].flags & MESH_RM_POSE_BINDING_FLAG) ||
                bindings[i].function != meshRmKeepInstancePose))) goto failedPose;
    }
    /* Validate first, then publish atomically. Binding order is depth-first,
       while native source indices can have parents later in the HIER array. */
    for (i = 0; i < count; ++i)
    {
        const unsigned int index = indices[i];
        bindings[i].object = &mesh->object[index];
        memcpy(&bindings[i].matrix, capture->nodes[index].localMatrix, 16 * sizeof(float));
        bindings[i].function = meshRmKeepInstancePose;
        bindings[i].flags |= MESH_RM_POSE_BINDING_FLAG;
    }
    free(capture);
    return TRUE;
failedPose:
    free(capture);
    return FALSE;
}
