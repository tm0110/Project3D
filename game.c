#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <SDL2/SDL.h>

#include "renderer.h"
#include "models.h"
#include "game.h"

typedef enum gmObjectType {
	GM_OBJECT_COMMON,
	GM_OBJECT_BLOOM,
	GM_OBJECT_SKIPSHADOWS,
	GM_OBJECT_SKIPSHADOWMAP
} gmObjectType;

typedef struct gmPoint gmPoint;
struct gmPoint
{
	float x;
	float z;
};

typedef struct gmNavRegion gmNavRegion;
struct gmNavRegion
{
	gmPoint upperLeft, upperRight;
	gmPoint lowerLeft, lowerRight;
};

typedef struct gmObject gmObject;
struct gmObject
{
	gmObjectType type;
	gmPoint      position;
	rdObject    *rObj;
};

typedef struct gmSector gmSector;
struct gmSector
{
	gmObject bulkObject;
	gmObject *decorationObject, _decorationObject;
	gmObject objects[8];
	int      numObjects;

	gmNavRegion navRegion;

	gmSector *link1, *link2;
};

typedef struct gmInputState gmInputState;
struct gmInputState
{
	int moveForwardBackward;
	int moveUpDown;
	int strafeRightLeft;
	int mouseX;
	int mouseY;
	int fullscreen;
	int toggleFullscreen;
	int quit;
	unsigned int timeDelta;
	unsigned int timeTotal;
	unsigned int timeLast;

	unsigned int numFrames;
	unsigned int timeSecond;
};

typedef struct gmGameState gmGameState;
struct gmGameState
{
	gmPoint playerPosition;
	gmPoint previousPlayerPosition;
};

typedef struct gmSkate gmSkate;
struct gmSkate
{
	int   skateOnOff;
	int   skateLeftRight;
	int   skateForwardBackward;
	float skateFactor;
};

static int  gm_HandleEvents(SDL_Window *window, gmInputState *state);
static void gm_HandleSingleEvent(const SDL_Event *ev, gmInputState *state);
static void gm_ToggleFullscreen(SDL_Window *window, int fullscreen);
static void gm_InitInputState(gmInputState *state);

static void      sr_SetupSector(gmSector *sector, gmObject *bulkObject, gmObject *decorationObject,
                           gmNavRegion navRegion);
static void      sr_AttachObject(gmSector *sector, gmObject *obj);
static void      sr_DrawSector(gmSector *sector);
static gmSector *sr_Collision(gmSector *currSector, gmPoint *playerPosition,
                              gmPoint *previousPlayerPosition);
static int       sr_IsInRegion(const gmNavRegion *region, const gmPoint *point, float xPad,
                               float zPad);

void gm_Main(SDL_Window *window)
{
	gmInputState inputState, inputStateCopy;

	gmGameState gameState;

	rdObject *bulkSouth;
	rdObject *bulkMid;
	rdObject *bulkNorth;
	rdObject *bulkConnect;
	rdObject *bulkRoom;
	rdObject *decorationSouth;
	rdObject *decorationMid;
	rdObject *decorationNorth;
	rdObject *decorationConnect;
	rdObject *decorationRoom;
	rdObject *flatCylinder;
	rdObject *flatCylinder2;
	rdObject *flatCylinder3;
	rdObject *flatCylinder4;
	rdObject *flatCylinder5;
	rdObject *flatCylinder6;
	rdObject *sphere;
	rdObject *sphere2;
	rdObject *sphere3;
	rdObject *sphere4;
	rdObject *sphere5;
	rdObject *riserSouth;
	rdObject *riserMid;
	rdObject *riserRoom;
	rdObject *teapot;

	rdShadowMap *shadowMapSouth;
	rdShadowMap *shadowMapMid;
	rdShadowMap *shadowMapRoom;

	gameState.playerPosition.x = 0.0f;
	gameState.playerPosition.z = 2.0f;

	bulkSouth = rd_CreateObject(sizeof (modelBulkSouth) / sizeof (rdVertex), modelBulkSouth, 0,
	                            NULL, RD_OBJECT_INTERIOR, RD_MATERIAL_PAINTJOB);

	decorationSouth = rd_CreateObject(sizeof (modelDecorationSouth) / sizeof (rdVertex),
	                                  modelDecorationSouth, 0, NULL, RD_OBJECT_EXTERIOR,
	                                  RD_MATERIAL_COMMON);

	bulkMid = rd_CreateObject(sizeof (modelBulkMid) / sizeof (rdVertex), modelBulkMid, 0, NULL,
	                          RD_OBJECT_INTERIOR, RD_MATERIAL_PAINTJOB);

	decorationMid = rd_CreateObject(sizeof (modelDecorationMid) / sizeof (rdVertex),
	                                modelDecorationMid, 0, NULL, RD_OBJECT_EXTERIOR,
	                                RD_MATERIAL_COMMON);

	bulkNorth = rd_CreateObject(sizeof (modelBulkNorth) / sizeof (rdVertex), modelBulkNorth, 0,
	                            NULL, RD_OBJECT_INTERIOR, RD_MATERIAL_PAINTJOB);

	decorationNorth = rd_CreateObject(sizeof (modelDecorationNorth) / sizeof (rdVertex),
		                              modelDecorationNorth, 0, NULL, RD_OBJECT_EXTERIOR,
		                              RD_MATERIAL_COMMON);

	bulkConnect = rd_CreateObject(sizeof (modelBulkConnect) / sizeof (rdVertex), modelBulkConnect,
	                              0, NULL, RD_OBJECT_INTERIOR, RD_MATERIAL_PAINTJOB);

	decorationConnect = rd_CreateObject(sizeof (modelDecorationConnect) / sizeof (rdVertex),
	                                    modelDecorationConnect, 0, NULL, RD_OBJECT_EXTERIOR,
	                                    RD_MATERIAL_COMMON);

	bulkRoom = rd_CreateObject(sizeof (modelBulkRoom) / sizeof (rdVertex), modelBulkRoom, 0, NULL,
	                           RD_OBJECT_INTERIOR, RD_MATERIAL_PAINTJOB);

	decorationRoom = rd_CreateObject(sizeof (modelDecorationRoom) / sizeof (rdVertex),
	                                 modelDecorationRoom, 0, NULL, RD_OBJECT_EXTERIOR,
	                                 RD_MATERIAL_COMMON);

	flatCylinder = rd_CreateObject(sizeof (modelFlatCylinderVertices) / sizeof (rdVertex),
	                               modelFlatCylinderVertices,
	                               sizeof (modelFlatCylinderIndices) / sizeof (rdIndex),
	                               modelFlatCylinderIndices, RD_OBJECT_EXTERIOR, RD_MATERIAL_BLOOM);

	flatCylinder2 = rd_CloneObject(flatCylinder);
	flatCylinder3 = rd_CloneObject(flatCylinder);
	flatCylinder4 = rd_CloneObject(flatCylinder);
	flatCylinder5 = rd_CloneObject(flatCylinder);
	flatCylinder6 = rd_CloneObject(flatCylinder);

	sphere = rd_CreateObject(sizeof (modelSphereVertices) / sizeof (rdVertex),
	                               modelSphereVertices, sizeof (modelSphereIndices) / sizeof (rdIndex),
	                               modelSphereIndices, RD_OBJECT_EXTERIOR, RD_MATERIAL_COMMON);

	sphere2 = rd_CloneObject(sphere);
	sphere3 = rd_CloneObject(sphere);
	sphere4 = rd_CloneObject(sphere);
	sphere5 = rd_CloneObject(sphere);

	riserSouth = rd_CreateObject(sizeof (modelRiserSouth) / sizeof (rdVertex), modelRiserSouth, 0,
	                             NULL, RD_OBJECT_EXTERIOR, RD_MATERIAL_COMMON);

	riserMid = rd_CreateObject(sizeof (modelRiserMid) / sizeof (rdVertex), modelRiserMid, 0, NULL,
	                           RD_OBJECT_EXTERIOR, RD_MATERIAL_COMMON);

	riserRoom = rd_CreateObject(sizeof (modelRiserRoom) / sizeof (rdVertex), modelRiserRoom, 0,
	                            NULL, RD_OBJECT_EXTERIOR, RD_MATERIAL_COMMON);

	teapot = rd_CreateObject(sizeof (modelTeapotVertices) / sizeof (rdVertex), modelTeapotVertices,
	                         sizeof (modelTeapotIndices) / sizeof (rdIndex), modelTeapotIndices,
	                         RD_OBJECT_EXTERIOR, RD_MATERIAL_COMMON);

	rd_SetLight(0, 0.0f, 3.98f, 0.0f, 0.7f, 0.7f, 1.0f, 100.0f, 9.0f, 0.0f);
	rd_SetLight(1, 0.0f, 2.28f, -0.25f, 0.7f, 0.7f, 1.0f, 16.0f, 7.5f, 1.0f);
	rd_SetLight(2, 0.0f, 3.98f, 8.0f, 1.0f, 1.0f, 1.0f, 100.0f, 13.5f, 0.0f);
	rd_SetLight(3, 0.0f, 2.28f, 8.0f, 1.0f, 1.0f, 1.0f, 16.0f, 13.5f, 1.0f);
	rd_SetLight(4, 8.0f, 3.98f, 14.0f, 1.0f, 0.7f, 0.7f, 100.0f, 7.5f, 0.0f);
	rd_SetLight(5, 8.0f, 2.28f, 14.0f, 1.0f, 0.7f, 0.7f, 16.0f, 5.5f, 1.0f);
	rd_SetLight(6, 8.0f, 3.98f, 8.0f, 1.0f, 1.0f, 1.0f, 100.0f, 11.5f, 0.0f);
	rd_SetLight(7, 8.0f, 2.28f, 8.0f, 1.0f, 1.0f, 1.0f, 16.0f, 11.5f, 1.0f);
	rd_SetLight(8, 15.0f, 3.98f, 20.0f, 0.7f, 0.7f, 1.0f, 30.0f, 13.5f, 0.0f);
	rd_SetLight(9, 15.0f, 2.28f, 20.0f, 0.7f, 0.7f, 1.0f, 6.5f, 13.5f, 1.0f);
	rd_SetLight(10, 8.0f, 3.98f, 20.0f, 1.0f, 1.0f, 1.0f, 100.0f, 10.0f, 0.0f);
	rd_SetLight(11, 8.0f, 2.28f, 20.0f, 1.0f, 1.0f, 1.0f, 16.0f, 10.0f, 1.0f);

	rd_EnableLight(0);
	rd_EnableLight(1);
	rd_EnableLight(2);
	rd_EnableLight(3);
	rd_EnableLight(4);
	rd_EnableLight(5);
	rd_EnableLight(6);
	rd_EnableLight(7);
	rd_EnableLight(8);
	rd_EnableLight(9);
	rd_EnableLight(10);
	rd_EnableLight(11);

	rd_SetMaterial(0, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.04f, 0.04f);
	rd_SetMaterial(1, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.04f, 0.0f);
	rd_SetMaterial(2, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.04f, 0.0f);
	rd_SetMaterial(3, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.08f, 0.0f);
	rd_SetMaterial(4, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.08f, 0.0f);

	rd_SetMaterial(5, 1.0f, 0.66f, 0.00f, 0.76f, 0.16f, 0.12f, 0.11f);
	rd_SetMaterial(6, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.06f, 0.0f);
	rd_SetMaterial(7, 0.10f, 0.05f, 0.10f, 0.76f, 0.16f, 0.0f, 0.12f);
	rd_SetMaterial(8, 0.50f, 1.00f, 0.25f, 0.76f, 0.16f, 0.03f, 0.12f);
	rd_SetMaterial(9, 1.00f, 0.25f, 0.55f, 0.76f, 0.16f, 0.03f, 0.12f);
	rd_SetMaterial(10, 0.65f, 0.40f, 0.50f, 0.76f, 0.16f, 0.06f, 0.02f);

	rd_SetMaterial(11, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.04f, 0.04f);
	rd_SetMaterial(12, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.04f, 0.0f);
	rd_SetMaterial(13, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.04f, 0.0f);
	rd_SetMaterial(14, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.08f, 0.0f);
	rd_SetMaterial(15, 0.4f, 0.28f, 0.4f, 0.40f, 0.50f, 0.08f, 0.0f);

	rd_SetObjectMaterial(bulkRoom, 11);
	rd_SetObjectMaterial(decorationSouth, 6);
	rd_SetObjectMaterial(decorationMid, 6);
	rd_SetObjectMaterial(decorationNorth, 6);
	rd_SetObjectMaterial(decorationConnect, 6);
	rd_SetObjectMaterial(decorationRoom, 6);
	rd_SetObjectMaterial(sphere, 5);
	rd_SetObjectMaterial(sphere2, 5);
	rd_SetObjectMaterial(sphere3, 8);
	rd_SetObjectMaterial(sphere4, 8);
	rd_SetObjectMaterial(sphere5, 9);
	rd_SetObjectMaterial(riserSouth, 7);
	rd_SetObjectMaterial(riserMid, 7);
	rd_SetObjectMaterial(riserRoom, 7);
	rd_SetObjectMaterial(teapot, 10);

	shadowMapSouth = rd_CreateShadowMap(0, 2048, 2048, 0.0f, 1.0f, -2.3f, 4.0f, 4.0f, 2.75f, 12.5f);
	shadowMapMid   = rd_CreateShadowMap(2, 2048, 2048, -10.0f, 1.0f, 8.0f, 4.0f, 2.0f, 3.75f,
	                                    12.5f);
	shadowMapRoom  = rd_CreateShadowMap(10, 2048, 2048, 15.1f, 1.0f, 20.0f, 4.0f, 2.2f, 4.85f,
	                                    12.5f);

	rd_AttachShadowMap(bulkSouth, shadowMapSouth);
	rd_AttachShadowMap(decorationSouth, shadowMapSouth);
	rd_AttachShadowMap(sphere, shadowMapSouth);
	rd_AttachShadowMap(sphere2, shadowMapSouth);
	rd_AttachShadowMap(sphere3, shadowMapMid);
	rd_AttachShadowMap(sphere4, shadowMapMid);
	rd_AttachShadowMap(sphere5, shadowMapMid);
	rd_AttachShadowMap(riserSouth, shadowMapSouth);
	rd_AttachShadowMap(riserMid, shadowMapMid);
	rd_AttachShadowMap(riserRoom, shadowMapRoom);

	rd_AttachShadowMap(bulkMid, shadowMapMid);
	rd_AttachShadowMap(decorationMid, shadowMapMid);
	rd_AttachShadowMap(bulkRoom, shadowMapRoom);
	rd_AttachShadowMap(decorationRoom, shadowMapRoom);
	rd_AttachShadowMap(teapot, shadowMapRoom);

	gmSector    sectorSouth;
	gmObject    sectorSouthBulkObject;
	gmNavRegion sectorSouthNavRegion;

	gmObject southFlatCylinder, southSphere1, southSphere2, southDecoration, southRiser;

	sectorSouthBulkObject.type = GM_OBJECT_COMMON;
	sectorSouthBulkObject.rObj = bulkSouth;

	sectorSouthNavRegion.upperLeft.x  = -2.0f;
	sectorSouthNavRegion.upperLeft.z  =  6.0f;
	sectorSouthNavRegion.upperRight.x =  2.0f;
	sectorSouthNavRegion.upperRight.z =  6.0f;

	sectorSouthNavRegion.lowerLeft.x  = -2.0f;
	sectorSouthNavRegion.lowerLeft.z  = -6.0f;
	sectorSouthNavRegion.lowerRight.x =  2.0f;
	sectorSouthNavRegion.lowerRight.z = -6.0f;

	southFlatCylinder.type = GM_OBJECT_BLOOM;
	southFlatCylinder.rObj = flatCylinder;

	southSphere1.type = GM_OBJECT_COMMON;
	southSphere1.rObj = sphere;

	southSphere2.type = GM_OBJECT_COMMON;
	southSphere2.rObj = sphere2;

	southDecoration.type = GM_OBJECT_COMMON;
	southDecoration.rObj = decorationSouth;

	southRiser.type = GM_OBJECT_COMMON;
	southRiser.rObj = riserSouth;

	sr_SetupSector(&sectorSouth, &sectorSouthBulkObject, &southDecoration, sectorSouthNavRegion);
	sr_AttachObject(&sectorSouth, &southSphere1);
	sr_AttachObject(&sectorSouth, &southSphere2);
	sr_AttachObject(&sectorSouth, &southFlatCylinder);
	sr_AttachObject(&sectorSouth, &southRiser);

	gmSector sectorMid;
	gmNavRegion midNavRegion;
	gmObject sectorMidBulkObject, sectorMidDecorationObject;

	gmObject midFlatCylinder, midFlatCylinder2, midRiser, midSphere1, midSphere2, midSphere3;

	midNavRegion.upperLeft.x  = -10.0f;
	midNavRegion.upperLeft.z  =  -6.0f;
	midNavRegion.upperRight.x =  10.0f;
	midNavRegion.upperRight.z =  -6.0f;

	midNavRegion.lowerLeft.x  = -10.0f;
	midNavRegion.lowerLeft.z  = -10.0f;
	midNavRegion.lowerRight.x =  10.0f;
	midNavRegion.lowerRight.z = -10.0f;

	sectorMidBulkObject.type = GM_OBJECT_COMMON;
	sectorMidBulkObject.rObj = bulkMid;

	sectorMidDecorationObject.type = GM_OBJECT_COMMON;
	sectorMidDecorationObject.rObj = decorationMid;

	midFlatCylinder.type = GM_OBJECT_BLOOM;
	midFlatCylinder.rObj = flatCylinder2;

	midFlatCylinder2.type = GM_OBJECT_BLOOM;
	midFlatCylinder2.rObj = flatCylinder4;

	midRiser.type = GM_OBJECT_COMMON;
	midRiser.rObj = riserMid;

	midSphere1.type = GM_OBJECT_COMMON;
	midSphere1.rObj = sphere3;

	midSphere2.type = GM_OBJECT_COMMON;
	midSphere2.rObj = sphere4;

	midSphere3.type = GM_OBJECT_COMMON;
	midSphere3.rObj = sphere5;

	sr_SetupSector(&sectorMid, &sectorMidBulkObject, NULL, midNavRegion);
	sr_AttachObject(&sectorMid, &midFlatCylinder);
	sr_AttachObject(&sectorMid, &midFlatCylinder2);
	sr_AttachObject(&sectorMid, &midRiser);
	sr_AttachObject(&sectorMid, &sectorMidDecorationObject);
	sr_AttachObject(&sectorMid, &midSphere1);
	sr_AttachObject(&sectorMid, &midSphere2);
	sr_AttachObject(&sectorMid, &midSphere3);

	gmSector    sectorNorth;
	gmNavRegion northNavRegion;
	gmObject    sectorNorthBulkObject, sectorNorthDecorationObject;
	gmObject    northFlatCylinder, northFlatCylinder2;

	northNavRegion.lowerRight.x =  10.0f;
	northNavRegion.lowerRight.z = -22.0f;
	northNavRegion.upperRight.x =  10.0f;
	northNavRegion.upperRight.z = -10.0f;

	northNavRegion.lowerLeft.x  =   6.0f;
	northNavRegion.lowerLeft.z  = -22.0f;
	northNavRegion.upperLeft.x  =   6.0f;
	northNavRegion.upperLeft.z  = -10.0f;

	sectorNorthBulkObject.type = GM_OBJECT_SKIPSHADOWMAP;
	sectorNorthBulkObject.rObj = bulkNorth;

	sectorNorthDecorationObject.type = GM_OBJECT_SKIPSHADOWMAP;
	sectorNorthDecorationObject.rObj = decorationNorth;

	northFlatCylinder.type = GM_OBJECT_BLOOM;
	northFlatCylinder.rObj = flatCylinder3;

	northFlatCylinder2.type = GM_OBJECT_BLOOM;
	northFlatCylinder2.rObj = flatCylinder6;

	sr_SetupSector(&sectorNorth, &sectorNorthBulkObject, &sectorNorthDecorationObject,
	               northNavRegion);
	sr_AttachObject(&sectorNorth, &northFlatCylinder);
	sr_AttachObject(&sectorNorth, &northFlatCylinder2);

	gmSector    sectorConnect;
	gmNavRegion connectNavRegion;
	gmObject    sectorConnectBulkObject, sectorConnectDecorationObject;

	connectNavRegion.lowerRight.x =  13.0f;
	connectNavRegion.lowerRight.z = -22.0f;
	connectNavRegion.upperRight.x =  13.0f;
	connectNavRegion.upperRight.z = -18.0f;

	connectNavRegion.lowerLeft.x  =   9.0f;
	connectNavRegion.lowerLeft.z  = -22.0f;
	connectNavRegion.upperLeft.x  =   9.0f;
	connectNavRegion.upperLeft.z  = -18.0f;

	sectorConnectBulkObject.type = GM_OBJECT_SKIPSHADOWMAP;
	sectorConnectBulkObject.rObj = bulkConnect;

	sectorConnectDecorationObject.type = GM_OBJECT_SKIPSHADOWMAP;
	sectorConnectDecorationObject.rObj = decorationConnect;

	sr_SetupSector(&sectorConnect, &sectorConnectBulkObject, &sectorConnectDecorationObject,
	               connectNavRegion);

	gmSector    sectorRoom;
	gmNavRegion roomNavRegion;
	gmObject    sectorRoomBulkObject, sectorRoomDecorationObject;
	gmObject    roomFlatCylinder, roomRiser, roomTeapot;

	roomNavRegion.lowerRight.x =  18.0f;
	roomNavRegion.lowerRight.z = -23.0f;
	roomNavRegion.upperRight.x =  18.0f;
	roomNavRegion.upperRight.z = -17.0f;

	roomNavRegion.lowerLeft.x  =  12.0f;
	roomNavRegion.lowerLeft.z  = -23.0f;
	roomNavRegion.upperLeft.x  =  12.0f;
	roomNavRegion.upperLeft.z  = -17.0f;

	sectorRoomBulkObject.type = GM_OBJECT_COMMON;
	sectorRoomBulkObject.rObj = bulkRoom;

	sectorRoomDecorationObject.type = GM_OBJECT_COMMON;
	sectorRoomDecorationObject.rObj = decorationRoom;

	roomFlatCylinder.type = GM_OBJECT_BLOOM;
	roomFlatCylinder.rObj = flatCylinder5;

	roomRiser.type = GM_OBJECT_COMMON;
	roomRiser.rObj = riserRoom;

	roomTeapot.type = GM_OBJECT_COMMON;
	roomTeapot.rObj = teapot;

	sr_SetupSector(&sectorRoom, &sectorRoomBulkObject, &sectorRoomDecorationObject, roomNavRegion);
	sr_AttachObject(&sectorRoom, &roomFlatCylinder);
	sr_AttachObject(&sectorRoom, &roomRiser);
	sr_AttachObject(&sectorRoom, &roomTeapot);

	sectorSouth.link1   = &sectorMid;
	sectorMid.link1     = &sectorSouth;
	sectorMid.link2     = &sectorNorth;
	sectorNorth.link1   = &sectorMid;
	sectorNorth.link2   = &sectorConnect;
	sectorConnect.link1 = &sectorNorth;
	sectorConnect.link2 = &sectorRoom;

	sectorRoom.link1 = &sectorConnect;

	gmSector *actualSector = &sectorSouth;

	rd_ResetDefaultCamera();
	rd_OrientDefaultCamera(180.0f, 0.0f);
	rd_ScaleObject(flatCylinder, 0.233f);
	rd_ScaleObject(flatCylinder2, 0.233f);
	rd_ScaleObject(flatCylinder3, 0.233f);
	rd_ScaleObject(flatCylinder4, 0.233f);
	rd_ScaleObject(flatCylinder5, 0.233f);
	rd_ScaleObject(flatCylinder6, 0.233f);
	rd_ScaleObject(sphere, 1.0f);
	rd_ScaleObject(sphere4, 1.00f);
	rd_ScaleObject(sphere5, 0.33f);
	rd_ScaleObject(teapot, 0.85f);
	rd_PositionObject(flatCylinder, 0.0f, 4.00f, 0.0f);
	rd_PositionObject(flatCylinder2, 0.0f, 4.0f, -8.0f);
	rd_PositionObject(flatCylinder3, 8.0f, 4.0f, -14.0f);
	rd_PositionObject(flatCylinder4, 8.0f, 4.0f, -8.0f);
	rd_PositionObject(flatCylinder5, 15.0f, 4.0f, -20.0f);
	rd_PositionObject(flatCylinder6, 8.0f, 4.0f, -20.0f);
	rd_PositionObject(sphere, -1.0f, 1.02f, 5.0f);
	rd_PositionObject(sphere2, 1.0f, 1.02f, 5.0f);
	rd_PositionObject(sphere3, -8.85f, 1.02f, -9.00f);
	rd_PositionObject(sphere4, -8.85f, 1.02f, -7.00f);
	rd_PositionObject(sphere5, -8.20f, 0.37f, -8.00f);
	rd_PositionObject(riserSouth, 0.0f, 0.0f, 5.15f);
	rd_PositionObject(riserMid, -9.125f, 0.0f, -8.0f);
	rd_PositionObject(riserRoom, 15.0f, 0.0f, -20.0f);
	rd_PositionObject(bulkMid, 0.0f, 0.0f, -8.0f);
	rd_PositionObject(decorationMid, 0.0f, 0.0f, -8.0f);
	rd_PositionObject(bulkNorth, 8.0f, 0.0f, -16.0f);
	rd_PositionObject(decorationNorth, 8.0f, 0.0f, -16.0f);
	rd_PositionObject(bulkConnect, 11.0f, 0.0f, -20.0f);
	rd_PositionObject(decorationConnect, 11.0f, 0.0f, -20.0f);
	rd_PositionObject(bulkRoom, 15.0f, 0.0f, -20.0f);
	rd_PositionObject(decorationRoom, 15.0f, 0.0f, -20.0f);
	rd_PositionObject(teapot, 15.0f, 0.05f, -20.0f);

	rd_PositionDefaultCamera(gameState.playerPosition.x, 2.9f, gameState.playerPosition.z);

	gm_InitInputState(&inputState);
	gm_InitInputState(&inputStateCopy);

	gmSkate skate;

	skate.skateOnOff           = 0;
	skate.skateLeftRight       = 0;
	skate.skateForwardBackward = 0;
	skate.skateFactor          = 0.0f;

	while (gm_HandleEvents(window, &inputState)) {
		const float moveFactor = 0.019f * inputState.timeDelta;
		const float mouseSens  = 0.061f;

		if (inputStateCopy.moveForwardBackward != 0 && inputState.moveForwardBackward == 0) {
			skate.skateOnOff           = 1;
			skate.skateForwardBackward = inputStateCopy.moveForwardBackward;
			skate.skateLeftRight       = 0;
			skate.skateFactor          = 1.0f;
		}

		if (inputStateCopy.strafeRightLeft != 0 && inputState.strafeRightLeft == 0) {
			skate.skateOnOff           = 1;
			skate.skateLeftRight       = inputStateCopy.strafeRightLeft;
			skate.skateForwardBackward = 0;
			skate.skateFactor          = 1.0f;
		}

		if (skate.skateOnOff == 1 && skate.skateFactor > 0.0f)
			skate.skateFactor -= 0.004f * inputState.timeDelta;

		rd_GetDefaultCameraPosition(&gameState.previousPlayerPosition.x, NULL,
		                            &gameState.previousPlayerPosition.z);

		if (inputState.strafeRightLeft != 0 || inputState.moveForwardBackward != 0) {
			skate.skateOnOff  = 0;
			skate.skateFactor = 0.0f;
			rd_MoveDefaultCameraFacingDirection(inputState.strafeRightLeft * moveFactor,
			                                    inputState.moveForwardBackward * moveFactor);
		} else if (skate.skateFactor >= 0.0f) {
			float sm = skate.skateFactor * moveFactor;

			rd_MoveDefaultCameraFacingDirection(skate.skateLeftRight * sm,
			                                    skate.skateForwardBackward * sm);
		}

		if (inputState.moveUpDown != 0)
			rd_MoveDefaultCamera(0.0f, moveFactor * inputState.moveUpDown, 0.0f);
		if (inputState.mouseX != 0 || inputState.mouseY != 0)
			rd_RotateDefaultCamera(inputState.mouseX * mouseSens, -inputState.mouseY * mouseSens);

		rd_GetDefaultCameraPosition(&gameState.playerPosition.x, NULL, &gameState.playerPosition.z);

		actualSector = sr_Collision(actualSector, &gameState.playerPosition,
		                            &gameState.previousPlayerPosition);

		rd_ClearShadowMap(shadowMapSouth);
		rd_ClearShadowMap(shadowMapMid);
		rd_ClearShadowMap(shadowMapRoom);
		rd_Clear(RD_CLEAR_GBUFFER);
		rd_Clear(RD_CLEAR_SHADOWS);
		rd_Clear(RD_CLEAR_DEPTHVELOCITY);
		rd_Clear(RD_CLEAR_BLOOM);

		sr_DrawSector(&sectorSouth);
		sr_DrawSector(&sectorMid);
		sr_DrawSector(&sectorNorth);
		sr_DrawSector(&sectorConnect);
		sr_DrawSector(&sectorRoom);

 		rd_Frame();

		SDL_GL_SwapWindow(window);
		inputState.numFrames++;
		inputStateCopy = inputState;
	}
	rd_DestroyObject(bulkSouth);
	rd_DestroyObject(decorationSouth);
	rd_DestroyObject(sphere5);
	rd_DestroyObject(sphere4);
	rd_DestroyObject(sphere3);
	rd_DestroyObject(sphere2);
	rd_DestroyObject(sphere);
	rd_DestroyObject(riserSouth);
	rd_DestroyObject(riserMid);
	rd_DestroyObject(riserRoom);
	rd_DestroyObject(flatCylinder4);
	rd_DestroyObject(flatCylinder3);
	rd_DestroyObject(flatCylinder2);
	rd_DestroyObject(flatCylinder);
	rd_DestroyObject(bulkMid);
	rd_DestroyObject(decorationMid);
	rd_DestroyObject(bulkNorth);
	rd_DestroyObject(decorationNorth);
	rd_DestroyObject(bulkConnect);
	rd_DestroyObject(decorationConnect);
	rd_DestroyObject(bulkRoom);
	rd_DestroyObject(decorationRoom);
	rd_DestroyObject(teapot);
	rd_DestroyShadowMap(shadowMapSouth);
	rd_DestroyShadowMap(shadowMapMid);
	rd_DestroyShadowMap(shadowMapRoom);
}

static int gm_HandleEvents(SDL_Window *window, gmInputState *state)
{
	SDL_Event ev;

	state->mouseX = 0;
	state->mouseY = 0;

	while (SDL_PollEvent(&ev)) {
		gm_HandleSingleEvent(&ev, state);
	}
	
	if (state->quit)
		return 0;

	state->timeTotal = SDL_GetTicks();
	state->timeDelta = state->timeTotal - state->timeLast;
	state->timeLast  = state->timeTotal;

	state->timeSecond += state->timeDelta;

	if (state->timeSecond >= 1000) {
		printf("Frames per second: %u\n", state->numFrames);
		state->numFrames = 0;
		state->timeSecond = 0;
	}

	if (state->toggleFullscreen) {
		gm_ToggleFullscreen(window, state->fullscreen);	
		state->toggleFullscreen = 0;
	}

	return 1;
}

static void gm_HandleSingleEvent(const SDL_Event *ev, gmInputState *state)
{
	if (ev->type == SDL_KEYUP) {
		SDL_Scancode sc = ev->key.keysym.scancode;

		if (sc == SDL_SCANCODE_W || sc == SDL_SCANCODE_S)
			state->moveForwardBackward = 0;
		else if (sc == SDL_SCANCODE_A || sc == SDL_SCANCODE_D)
			state->strafeRightLeft = 0;
		else if (sc == SDL_SCANCODE_Z || sc == SDL_SCANCODE_X)
			state->moveUpDown = 0;

		return;
	}

	if (ev->type == SDL_KEYDOWN) {
		SDL_Scancode sc = ev->key.keysym.scancode;
		
		if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_Q)
			state->quit = 1;
		else if (sc == SDL_SCANCODE_W)
			state->moveForwardBackward = 1;
		else if (sc == SDL_SCANCODE_S)
			state->moveForwardBackward = -1;
		else if (sc == SDL_SCANCODE_A)
			state->strafeRightLeft = -1;
		else if (sc == SDL_SCANCODE_D)
			state->strafeRightLeft = 1;
		else if (sc == SDL_SCANCODE_Z)
			state->moveUpDown = -1;
		else if (sc == SDL_SCANCODE_X)
			state->moveUpDown = 1;
		else if (sc == SDL_SCANCODE_M) {
			state->fullscreen = (state->fullscreen != 1);
			state->toggleFullscreen = 1;
		}

		return;
	}

	if (ev->type == SDL_MOUSEMOTION) {
			state->mouseX += ev->motion.xrel;
			state->mouseY += ev->motion.yrel;
	}
}

static void gm_ToggleFullscreen(SDL_Window *window, int fullscreen)
{
	SDL_DisplayMode mode;

	if (fullscreen == 1) {
		SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_GetWindowDisplayMode(window, &mode);
		printf("Width = %d, height = %d\n", mode.w, mode.h);
		rd_Viewport(mode.w, mode.h);
	} else {
		SDL_SetWindowFullscreen(window, 0);
		SDL_GetWindowDisplayMode(window, &mode);
		rd_Viewport(mode.w, mode.h);
	}
}

static void gm_InitInputState(gmInputState *state)
{
	state->moveForwardBackward = 0;
	state->moveUpDown          = 0;
	state->strafeRightLeft     = 0;
	state->mouseX              = 0;
	state->mouseY              = 0;
	state->fullscreen          = 0;
	state->toggleFullscreen    = 0;
	state->quit                = 0;
	state->timeDelta           = 0;
	state->timeTotal           = 0;
	state->timeLast            = 0;
	state->numFrames           = 0;
	state->timeSecond          = 0;
}

static void sr_SetupSector(gmSector *sector, gmObject *bulkObject, gmObject *decorationObject,
                           gmNavRegion navRegion)
{
	sector->bulkObject = *bulkObject;

	if (decorationObject) {
		sector->_decorationObject = *decorationObject;
		sector->decorationObject = &sector->_decorationObject;
	} else
		sector->decorationObject = NULL;

	sector->navRegion = navRegion;

	sector->numObjects = 0;
	sector->link1 = NULL;
	sector->link2 = NULL;
}

static void sr_AttachObject(gmSector *sector, gmObject *obj)
{
	assert(sector->numObjects < 8);

	sector->objects[sector->numObjects] = *obj;
	sector->numObjects++;
}

static void sr_DrawSector(gmSector *sector)
{
	for (int i = -2; i < sector->numObjects; i++) {
		gmObject *obj;

		if (i == -2) {
			obj = sector->decorationObject;

			if (obj == NULL)
				continue;
		} else if (i == -1)
			obj = &sector->bulkObject;
		else
			obj = &sector->objects[i];

		if (obj->type == GM_OBJECT_SKIPSHADOWMAP || obj->type == GM_OBJECT_BLOOM)
			continue;

		rd_Draw(RD_DRAW_SHADOWMAP, obj->rObj);
	}

	for (int i = -2; i < sector->numObjects; i++) {
		gmObject *obj;

		if (i == -2) {
			obj = sector->decorationObject;

			if (obj == NULL)
				continue;
		} else if (i == -1)
			obj = &sector->bulkObject;
		else
			obj = &sector->objects[i];

		rd_Draw(RD_DRAW_DEPTHVELOCITY, obj->rObj);
		if (obj->type == GM_OBJECT_COMMON) {
			rd_Draw(RD_DRAW_SHADOWS, obj->rObj);
		}
		else if(obj->type == GM_OBJECT_BLOOM) {
			rd_Draw(RD_DRAW_BLOOM, obj->rObj);
		}

		if (obj->type != GM_OBJECT_BLOOM) {
			rd_Draw(RD_DRAW_GBUFFER, obj->rObj);
		}
	}
}

static gmSector *sr_Collision(gmSector *currSector, gmPoint *playerPosition,
                              gmPoint *previousPlayerPosition)
{
	if (!sr_IsInRegion(&currSector->navRegion, playerPosition, 0.45f, 0.45f)) {
		float playerHeight;
		gmPoint potentialPlayerPosition1, potentialPlayerPosition2;

		rd_GetDefaultCameraPosition(NULL, &playerHeight, NULL);

		potentialPlayerPosition1.x = playerPosition->x;
		potentialPlayerPosition1.z = previousPlayerPosition->z;

		potentialPlayerPosition2.x = previousPlayerPosition->x;
		potentialPlayerPosition2.z = playerPosition->z;

		if (currSector->link1 != NULL) {
			if (sr_IsInRegion(&currSector->link1->navRegion, playerPosition, 0.45f, -0.45f))
				return currSector->link1;
		}

		if (currSector->link2 != NULL) {
			if (sr_IsInRegion(&currSector->link2->navRegion, playerPosition, 0.45f, -0.45f))
				return currSector->link2;
		}

		if (sr_IsInRegion(&currSector->navRegion, &potentialPlayerPosition1, 0.45f, 0.45f))
			rd_PositionDefaultCamera(potentialPlayerPosition1.x, playerHeight,
			                         potentialPlayerPosition1.z);
		else if (sr_IsInRegion(&currSector->navRegion, &potentialPlayerPosition2, 0.45f, 0.45f))
			rd_PositionDefaultCamera(potentialPlayerPosition2.x, playerHeight,
			                         potentialPlayerPosition2.z);
		else
			rd_PositionDefaultCamera(previousPlayerPosition->x, playerHeight,
			                         previousPlayerPosition->z);
	}

	return currSector;	
}

static int sr_IsInRegion(const gmNavRegion *region, const gmPoint *point, float xPad, float zPad)
{
	if (region->upperLeft.x > point->x - xPad  || region->upperLeft.z < point->z + zPad)
		return 0;
	if (region->lowerLeft.x > point->x - xPad  || region->lowerLeft.z > point->z - zPad)
		return 0;
	if (region->upperRight.x < point->x + xPad || region->upperRight.z < point->z + zPad)
		return 0;
	if (region->lowerRight.x < point->x + xPad || region->lowerRight.z > point->z - zPad)
		return 0;
	return 1;
}
