#ifndef RENDERER_H
#define RENDERER_H

#include <stdlib.h>
#include "renderer_gldecl.h"

typedef enum rdClearType
{
	RD_CLEAR_DEPTHVELOCITY,
	RD_CLEAR_GBUFFER,
	RD_CLEAR_SHADOWS,
	RD_CLEAR_BLOOM
} rdClearType;

typedef enum rdDrawType
{
	RD_DRAW_DEPTHVELOCITY,
	RD_DRAW_SHADOWMAP,
	RD_DRAW_GBUFFER,
	RD_DRAW_SHADOWS,
	RD_DRAW_BLOOM,
	RD_DRAW_DEBUG_PREPASSDEPTH,
	RD_DRAW_DEBUG_VELOCITY,
	RD_DRAW_DEBUG_NORMALS,
	RD_DRAW_DEBUG_SHADOWMAP,
	RD_DRAW_DEBUG_BLOOM,
	RD_DRAW_DEBUG_REFLECTIONS
} rdDrawType;

typedef enum rdObjectType
{
	RD_OBJECT_EXTERIOR,
	RD_OBJECT_INTERIOR
} rdObjectType;

typedef enum rdMaterialType
{
	RD_MATERIAL_COMMON,
	RD_MATERIAL_PAINTJOB,
	RD_MATERIAL_BLOOM
} rdMaterialType;

typedef struct rdVertex rdVertex;
struct rdVertex
{
	float x;
	float y;
	float z;
};

typedef void *rdAlloc(size_t);
typedef void  rdFree(void *);

typedef unsigned short rdIndex;

typedef struct rdShadowMap rdShadowMap;
typedef struct rdObject    rdObject;

void rd_Init(rdGL gl_init, int width, int height);
void rd_Shutdown(void);
void rd_SetCustomAllocator(rdAlloc *alloc, rdFree *free);
void rd_Viewport(int width, int height);
void rd_Clear(rdClearType clear);
void rd_ClearShadowMap(const rdShadowMap *sw);
void rd_Draw(rdDrawType draw, rdObject *obj);
void rd_Frame(void);

void rd_SetLight(int index, float x, float y, float z, float red, float green, float blue,
                 float intensity, float cutoffRadius, float upward);
void rd_EnableLight(int index);
void rd_DisableLight(int index);

void rd_SetMaterial(int index, float red, float green, float blue, float metalness, float roughness,
                    float ambient, float reflectance);

rdShadowMap *rd_CreateShadowMap(int originLightIndex, int pixWidth, int pixHeight, float targetX,
                                float targetY, float targetZ, float spanWidth, float spanHeight,
                                float zNear, float zFar);
void         rd_DestroyShadowMap(rdShadowMap *sm);
void         rd_AttachShadowMap(rdObject *obj, rdShadowMap *sm);

rdObject *rd_CreateObject(int numVertices, const rdVertex *vertices, int numIndices,
                          const rdIndex *indices, rdObjectType objectType,
                          rdMaterialType materialType);
void      rd_DestroyObject(rdObject *obj);
rdObject *rd_CloneObject(rdObject *original);
void      rd_SetObjectMaterial(rdObject *obj, int materialID);
void      rd_ResetObject(rdObject *obj);
void      rd_PositionObject(rdObject *obj, float x, float y, float z);
void      rd_MoveObject(rdObject *obj, float x, float y, float z);
void      rd_ScaleObject(rdObject *obj, float scale);
void      rd_OrientObject(rdObject *obj, float x, float y, float z);
void      rd_RotateObject(rdObject *obj, float x, float y, float z);

void rd_ResetDefaultCamera(void);
void rd_PositionDefaultCamera(float x, float y, float z);
void rd_MoveDefaultCamera(float x, float y, float z);
void rd_MoveDefaultCameraFacingDirection(float right, float forward);
void rd_OrientDefaultCamera(float yaw, float pitch);
void rd_RotateDefaultCamera(float yaw, float pitch);
void rd_GetDefaultCameraPosition(float *x, float *y, float *z);
void rd_GetDefaultCameraOrientation(float *yaw, float *pitch);

#endif
