#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include "renderer.h"
#include "shaders.h"

#define RD_PI 3.1415926535897932384626433832795

typedef enum rdRenderState 
{
	RD_RENDERSTATE_FRESH,
	RD_RENDERSTATE_PARTIAL
} rdRenderState;

typedef struct rdMem rdMem;
struct rdMem
{
	rdAlloc *alloc;
	rdFree  *free;
};

typedef struct rdVec2 rdVec2;
struct rdVec2
{
	float x, y;
};

typedef struct rdVec3 rdVec3;
struct rdVec3
{
	float x, y, z;
};

typedef struct rdVec4 rdVec4;
struct rdVec4
{
	float x, y, z, w;
};

typedef struct rdMat3 rdMat3;
struct rdMat3
{
	float m[3][3];
};

typedef struct rdMat4 rdMat4;
struct rdMat4
{
	float m[4][4];
};

typedef struct rdTriangle rdTriangle;
struct rdTriangle
{
	rdVec3 v1, v2, v3;
};

typedef struct rdLight rdLight;
struct rdLight
{
	float x, y, z;
	float red, green, blue;
	float intensity;
	float cutoffRadius;
	float upward;

	int enabled;
};

typedef struct rdMaterial rdMaterial;
struct rdMaterial
{
	float red, green, blue;

	float metalness;
	float roughness;
	float ambient;
	float reflectance;
};

typedef struct rdCamera rdCamera;
struct rdCamera
{
	float x, y, z;
	float yaw, pitch;

	rdMat4 mView;
	int    update;
};

typedef struct rdShader rdShader;
struct rdShader
{
	GLuint vertexShader;
	GLuint fragmentShader;
	GLuint shaderProgram;

	GLint uniforms[16];
};

typedef struct rdQuad rdQuad;
struct rdQuad
{
	GLuint vertexBuffer;
	GLuint uvBuffer;
	GLuint indexBuffer;

	GLuint vertexArray;
};

typedef struct rdDepthVelocityBuffer rdDepthVelocityBuffer;
struct rdDepthVelocityBuffer
{
	GLuint framebuf;
	GLuint depthTexture;
	GLuint velocityTexture;
};

typedef struct rdColorBuffer rdColorBuffer;
struct rdColorBuffer
{
	int pixWidth, pixHeight;

	GLuint framebuf;
	GLuint colorTexture;
};

typedef struct rdGBuffer rdGBuffer;
struct rdGBuffer
{
	GLuint framebuf;
	GLuint materialIDTexture;
	GLuint normalTexture;
};

struct rdShadowMap
{
	int originLightIndex;
	int pixWidth, pixHeight;
	int numObjectsAttached;

	GLuint framebuf;
	GLuint depthTexture;

	rdMat4 mLightspace;

};

typedef struct rdShadowsBuffer rdShadowsBuffer;
struct rdShadowsBuffer
{
	GLuint framebuf;
	GLuint shadowsTexture;
};

typedef struct rdBloomBuffer rdBloomBuffer;
struct rdBloomBuffer
{
	int width, height;
	int blurWidth, blurHeight;

	GLuint framebufRaw;
	GLuint bloomRawTexture;

	GLuint framebufBlur1;
	GLuint bloomBlur1Texture;

	GLuint framebufBlur2;
	GLuint bloomBlur2Texture;
};

typedef struct rdSSAOBuffer rdSSAOBuffer;
struct rdSSAOBuffer
{
	int width, height;

	rdVec3 kernel[64];
	GLuint noiseTexture;

	GLuint framebufRaw;
	GLuint occlusionRawTexture;

	GLuint framebufBlur;
	GLuint occlusionBlurTexture;
};

struct rdObject
{
	rdObject *parent;
	int       numClones;

	rdObjectType   objectType;
	rdMaterialType materialType;
	int            materialID;

	int numVertices;
	int numIndices;

	float posX, posY, posZ;
	float scale;
	float rotX, rotY, rotZ;

	rdMat4 mModel;
	int    update;

	int    isIndexed;

	GLuint vertexBuffer;
	GLuint normalBuffer;
	GLuint indexBuffer;
	GLuint vertexArray;

	rdMat4 mMVP;
	rdMat4 *mPrevMVP, _mPrevMVP;

	rdVec3 lastCameraPosition;
	float  lastCameraYaw, lastCameraPitch;

	int jitterIndex;

	rdShadowMap *sm;
};

typedef struct rdLocal rdLocal;
struct rdLocal
{
	rdRenderState renderState;

	int screenWidth, screenHeight;

	rdLight    lights[64];
	rdMaterial materials[64];

	rdShader depthOnlyShader;
	rdShader depthVelocityShader;
	rdShader geometryShader;
	rdShader lightingShader;
	rdShader ssaoShader;
	rdShader shadowShader;
	rdShader bloomShader;
	rdShader ssrShader;
	rdShader compositeShader;
	rdShader tAAResolveMotionBlurShader;
	rdShader postProcessShader;
	rdShader blurSingleChannelShader;
	rdShader gaussianBlurSingleChannelShader;
	rdShader debugSingleChannelShader;
	rdShader debugDualChannelShader;
	rdShader debugTripleChannelShader;
	rdShader debugNormalsShader;

	rdDepthVelocityBuffer depthVelocityBuffer;
	rdGBuffer             gBuffer;

	rdColorBuffer colorBuffer;
	rdColorBuffer frontBuffer;
	rdColorBuffer backBuffer;
	rdColorBuffer reflectionsBuffer;

	rdSSAOBuffer    ssaoBuffer;
	rdShadowsBuffer shadowsBuffer;
	rdBloomBuffer   bloomBuffer;

	rdQuad screenQuad;

	rdCamera defaultCamera;

	rdMat4 mProjection;
	rdMat4 mProjectionJitter;
	rdMat4 mInvProjection;
};

static void cm_ResetCamera(rdCamera *cam);
static void cm_SyncViewMatrix(rdCamera *cam);
static int  cm_CheckCameraAttributes(const rdCamera *cam, const rdVec3 *pos, float yaw,
                                     float pitch);

static void fb_SetupQuad(rdQuad *quad);
static void fb_DestroyQuad(rdQuad *quad);

static void fb_SetupDepthVelocityBuffer(rdDepthVelocityBuffer *depthVelocityBuffer, int screenWidth,
                                        int screenHeight);
static void fb_DestroyDepthVelocityBuffer(rdDepthVelocityBuffer *depthVelocityBuffer);

static void fb_SetupGBuffer(rdGBuffer *gBuffer, const rdDepthVelocityBuffer *depthVelocityBuffer,
                            int screenWidth, int screenHeight);
static void fb_DestroyGBuffer(rdGBuffer *gBuffer);

static void fb_SetupColorBuffer(rdColorBuffer *colorBuffer, int width, int height);
static void fb_DestroyColorBuffer(rdColorBuffer *colorBuffer);

static void fb_SetupShadowsBuffer(rdShadowsBuffer *shadowsBuffer,
                                  const rdDepthVelocityBuffer *depthVelocityBuffer, int screenWidth,
                                  int screenHeight);
static void fb_DestroyShadowsBuffer(rdShadowsBuffer *shadowsBuffer);

static void fb_SetupBloomBuffer(rdBloomBuffer *bloomBuffer,
                                const rdDepthVelocityBuffer *depthVelocityBuffer, int width,
                                int height, int blurWidth, int blurHeight);
static void fb_DestroyBloomBuffer(rdBloomBuffer *bloomBuffer);

static void fb_SetupAmbientOcclusionBuffer(rdSSAOBuffer *ssaoBuffer, int width, int height);
static void fb_DestroyAmbientOcclusionBuffer(rdSSAOBuffer *ssaoBuffer);

static void sh_SetupShader(rdShader *shader, const char *sourceVertex, const char *sourceFragment);
static void sh_DestroyShader(rdShader *shader);
static void sh_SetupUniform(rdShader *shader, int index, const char *name);

static void me_GenerateNormalsIndexed(rdVec3 *outNormals, int numVertices, const rdVertex *vertices,
                                      int numIndices, const rdIndex *indices);
static void me_GenerateNormalsNonIndexed(rdVec3 *outNormals, int numVertices,
                                         const rdVertex *vertices);
static void me_InvertNormals(rdVec3 *normals, int numNormals);

static rdTriangle tr_FromVertices(const rdVec3 *v1, const rdVec3 *v2, const rdVec3 *v3);
static rdVec3     tr_Normal(const rdTriangle *tri);
static int        tr_IsAdjacent(const rdTriangle *tri, const rdVec3 *vec);

static void   mx_Identity(rdMat4 *mat);
static void   mx_Zero(rdMat4 *mat);
static void   mx_Translate(rdMat4 *mat, float x, float y, float z);
static void   mx_Scale(rdMat4 *mat, float x, float y, float z);
static void   mx_Rotate(rdMat4 *mat, float x, float y, float z);
static void   mx_LookAt(rdMat4 *mat, const rdVec3 *position, const rdVec3 *target,
                        const rdVec3 *up);
static void   mx_Frustum(rdMat4 *p, double left, double right, double bottom, double top,
                         double zNear, double zFar);
static void   mx_InvertFrustum(rdMat4 *p);
#if 0
static void   mx_Ortho(rdMat4 *o, double left, double right, double bottom, double top, double near,
                       double far);
#endif
static void   mx_MultiAB(rdMat4 *out, const rdMat4 *a, const rdMat4 *b);
static void   mx_MultiABC(rdMat4 *out, const rdMat4 *a, const rdMat4 *b, const rdMat4 *c);
static rdVec3 mx_MultiVector3(const rdMat3 *mat, const rdVec3 *vec);
static rdVec4 mx_MultiVector4(const rdMat4 *mat, const rdVec4 *vec);
static rdMat3 mx_Mat3From4(const rdMat4 *src);
static void   mx_Invert3(rdMat3 *mat);
static void   mx_Transpose3(rdMat3 *mat);

static rdVec2 vc_Vec2(float x, float y);
static rdVec3 vc_Vec3(float x, float y, float z);
static rdVec4 vc_Vec4(float x, float y, float z, float w);
static void   vc_Zero(rdVec3 *vec);
static rdVec3 vc_Add(const rdVec3 *a, const rdVec3 *b);
static rdVec3 vc_Sub(const rdVec3 *a, const rdVec3 *b);
static rdVec3 vc_Multiply(const rdVec3 *a, const rdVec3 *b);
static rdVec3 vc_MultiScalar(const rdVec3 *vec, float scalar);
#if 0
static rdVec3 vc_DivideScalar(const rdVec3 *vec, float scalar);
#endif
static int    vc_Equal(const rdVec3 *a, const rdVec3 *b);
static rdVec3 vc_Cross(const rdVec3 *a, const rdVec3 *b);
static float  vc_Dot(const rdVec3 *a, const rdVec3 *b);
static void   vc_Normalize(rdVec3 *vec);
static void   vc_Invert(rdVec3 *vec);

static float ma_ToRadians(float degrees);
static float ma_WrapAngle(float angle);
static float ma_Clamp(float val, float min, float max);
static float ma_Random(float min, float max);
static float ma_Lerp(float a, float b, float f);
static float ma_Halton(unsigned int i, unsigned int base);

static rdMem   mem;
static rdGL    gl;
static rdLocal local;

void rd_Init(rdGL gl_init, int width, int height)
{
	mem.alloc = malloc;
	mem.free  = free;

	gl = gl_init;

	assert(sizeof(float) == sizeof(GLfloat));
	assert(sizeof(unsigned short) == sizeof(GLushort));

	printf("Initializing renderer...\n");
	printf("OpenGL version: %s\n", gl.GetString(GL_VERSION));
	printf("                %s\n", gl.GetString(GL_RENDERER));
	printf("                %s\n", gl.GetString(GL_VENDOR));

	gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	gl.Enable(GL_DEPTH_TEST);
	gl.Enable(GL_CULL_FACE);
	gl.DepthFunc(GL_LESS);

	local.renderState = RD_RENDERSTATE_FRESH;

	local.screenWidth  = 2;
	local.screenHeight = 2;

	cm_ResetCamera(&local.defaultCamera);
	mx_Identity(&local.mProjection);

	for (int i = 0; i < 64; i++) {
		rdLight *l = &local.lights[i];
		
		l->x = 0.0f;
		l->y = 0.0f;
		l->z = 0.0f;

		l->red   = 1.0f;
		l->green = 1.0f;
		l->blue  = 1.0f;

		l->intensity    = 30.0f;
		l->cutoffRadius = 1000.0f;
		l->upward       = 0.0f;

		l->enabled = 0;
	}

	for (int i = 0; i < 64; i++) {
		rdMaterial *m = &local.materials[i];

		m->red   = 0.50f;
		m->green = 0.50f;
		m->blue  = 0.50f;

		m->metalness = 0.50f;
		m->roughness = 0.50f;

		m->ambient     = 0.00f;
		m->reflectance = 0.00f;
	}

	sh_SetupShader(&local.depthOnlyShader, shaderSourceDepthOnlyVertex,
	               shaderSourceDepthOnlyFragment);
	sh_SetupUniform(&local.depthOnlyShader, 0, "mMVP");

	sh_SetupShader(&local.depthVelocityShader, shaderSourceDepthVelocityVertex,
	               shaderSourceDepthVelocityFragment);
	sh_SetupUniform(&local.depthVelocityShader, 0, "mMVP");
	sh_SetupUniform(&local.depthVelocityShader, 1, "mPrevMVP");
	sh_SetupUniform(&local.depthVelocityShader, 2, "currJitter");
	sh_SetupUniform(&local.depthVelocityShader, 3, "prevJitter");

	sh_SetupShader(&local.geometryShader, shaderSourceGeometryVertex, shaderSourceGeometryFragment);
	sh_SetupUniform(&local.geometryShader, 0, "mModelView");
	sh_SetupUniform(&local.geometryShader, 1, "mMVP");
	sh_SetupUniform(&local.geometryShader, 2, "mNormal");
	sh_SetupUniform(&local.geometryShader, 3, "paintjob");
	sh_SetupUniform(&local.geometryShader, 4, "materialID");

	sh_SetupShader(&local.lightingShader, shaderSourceLightingVertex, shaderSourceLightingFragment);
	sh_SetupUniform(&local.lightingShader, 0, "depthTexture");
	sh_SetupUniform(&local.lightingShader, 1, "materialIDTexture");
	sh_SetupUniform(&local.lightingShader, 2, "normalTexture");
	sh_SetupUniform(&local.lightingShader, 3, "occlusionTexture");
	sh_SetupUniform(&local.lightingShader, 4, "shadowsTexture");
	sh_SetupUniform(&local.lightingShader, 5, "bloomRawTexture");
	sh_SetupUniform(&local.lightingShader, 6, "bloomBlurredTexture");
	sh_SetupUniform(&local.lightingShader, 7, "mInvProjection");
	sh_SetupUniform(&local.lightingShader, 8, "viewspaceUp");
	sh_SetupUniform(&local.lightingShader, 9, "materialColors");
	sh_SetupUniform(&local.lightingShader, 10, "materialProperties");
	sh_SetupUniform(&local.lightingShader, 11, "numLights");
	sh_SetupUniform(&local.lightingShader, 12, "lightPositions");
	sh_SetupUniform(&local.lightingShader, 13, "lightColors");
	sh_SetupUniform(&local.lightingShader, 14, "lightProperties");

	sh_SetupShader(&local.ssaoShader, shaderSourceSSAOVertex, shaderSourceSSAOFragment);
	sh_SetupUniform(&local.ssaoShader, 0, "depthTexture");
	sh_SetupUniform(&local.ssaoShader, 1, "normalTexture");
	sh_SetupUniform(&local.ssaoShader, 2, "noiseTexture");
	sh_SetupUniform(&local.ssaoShader, 3, "mProjection");
	sh_SetupUniform(&local.ssaoShader, 4, "mInvProjection");
	sh_SetupUniform(&local.ssaoShader, 5, "samples");
	sh_SetupUniform(&local.ssaoShader, 6, "resolution");

	sh_SetupShader(&local.shadowShader, shaderSourceShadowVertex, shaderSourceShadowFragment);
	sh_SetupUniform(&local.shadowShader, 0, "mMVP");
	sh_SetupUniform(&local.shadowShader, 1, "mNormal");
	sh_SetupUniform(&local.shadowShader, 2, "mModelView");
	sh_SetupUniform(&local.shadowShader, 3, "mModelLightspace");
	sh_SetupUniform(&local.shadowShader, 4, "shadowMapTexture");
	sh_SetupUniform(&local.shadowShader, 5, "actualLightPosition");

	sh_SetupShader(&local.bloomShader, shaderSourceBloomVertex, shaderSourceBloomFragment);
	sh_SetupUniform(&local.bloomShader, 0, "mMVP");

	sh_SetupShader(&local.ssrShader, shaderSourceSSRVertex, shaderSourceSSRFragment);
	sh_SetupUniform(&local.ssrShader, 0, "mProjection");
	sh_SetupUniform(&local.ssrShader, 1, "mInvProjection");
	sh_SetupUniform(&local.ssrShader, 2, "depthTexture");
	sh_SetupUniform(&local.ssrShader, 3, "materialIDTexture");
	sh_SetupUniform(&local.ssrShader, 4, "normalTexture");
	sh_SetupUniform(&local.ssrShader, 5, "colorTexture");
	sh_SetupUniform(&local.ssrShader, 6, "reflectanceProperties");

	sh_SetupShader(&local.compositeShader, shaderSourceCompositeVertex,
	               shaderSourceCompositeFragment);
	sh_SetupUniform(&local.compositeShader, 0, "colorTexture");
	sh_SetupUniform(&local.compositeShader, 1, "reflectionsTexture");
	sh_SetupUniform(&local.compositeShader, 2, "materialIDTexture");
	sh_SetupUniform(&local.compositeShader, 3, "reflectanceProperties");

	sh_SetupShader(&local.tAAResolveMotionBlurShader, shaderSourceTAAResolveMotionBlurVertex,
	               shaderSourceTAAResolveMotionBlurFragment);
	sh_SetupUniform(&local.tAAResolveMotionBlurShader, 0, "colorTexture");
	sh_SetupUniform(&local.tAAResolveMotionBlurShader, 1, "prevColorTexture");
	sh_SetupUniform(&local.tAAResolveMotionBlurShader, 2, "depthTexture");
	sh_SetupUniform(&local.tAAResolveMotionBlurShader, 3, "velocityTexture");
	sh_SetupUniform(&local.tAAResolveMotionBlurShader, 4, "resolution");

	sh_SetupShader(&local.postProcessShader, shaderSourcePostProcessVertex,
	               shaderSourcePostProcessFragment);
	sh_SetupUniform(&local.postProcessShader, 0, "colorTexture");
	sh_SetupUniform(&local.postProcessShader, 1, "randomInput");	
	sh_SetupUniform(&local.postProcessShader, 2, "resolution");
	sh_SetupUniform(&local.postProcessShader, 3, "lensFlareEnabled");
	sh_SetupUniform(&local.postProcessShader, 4, "lensFlarePos");

	sh_SetupShader(&local.blurSingleChannelShader, shaderSourceBlurSingleChannelVertex,
	               shaderSourceBlurSingleChannelFragment);
	sh_SetupUniform(&local.blurSingleChannelShader, 0, "inputTexture");

	sh_SetupShader(&local.gaussianBlurSingleChannelShader,
	               shaderSourceGaussianBlurSingleChannelVertex,
	               shaderSourceGaussianBlurSingleChannelFragment);
	sh_SetupUniform(&local.gaussianBlurSingleChannelShader, 0, "inputTexture");
	sh_SetupUniform(&local.gaussianBlurSingleChannelShader, 1, "horizontal");

	sh_SetupShader(&local.debugSingleChannelShader, shaderSourceDebugSingleChannelVertex,
	               shaderSourceDebugSingleChannelFragment);
	sh_SetupUniform(&local.debugSingleChannelShader, 0, "inputTexture");

	sh_SetupShader(&local.debugDualChannelShader, shaderSourceDebugDualChannelVertex,
	               shaderSourceDebugDualChannelFragment);
	sh_SetupUniform(&local.debugDualChannelShader, 0, "inputTexture");

	sh_SetupShader(&local.debugTripleChannelShader, shaderSourceDebugTripleChannelVertex,
	               shaderSourceDebugTripleChannelFragment);
	sh_SetupUniform(&local.debugTripleChannelShader, 0, "inputTexture");

	sh_SetupShader(&local.debugNormalsShader, shaderSourceDebugNormalsVertex,
	               shaderSourceDebugNormalsFragment);

	fb_SetupDepthVelocityBuffer(&local.depthVelocityBuffer, 128, 128);
	fb_SetupGBuffer(&local.gBuffer, &local.depthVelocityBuffer, 128, 128);

	fb_SetupColorBuffer(&local.colorBuffer, 128, 128);
	fb_SetupColorBuffer(&local.frontBuffer, 128, 128);
	fb_SetupColorBuffer(&local.backBuffer, 128, 128);
	fb_SetupColorBuffer(&local.reflectionsBuffer, 128, 128);

	fb_SetupAmbientOcclusionBuffer(&local.ssaoBuffer, 128, 128);
	fb_SetupShadowsBuffer(&local.shadowsBuffer, &local.depthVelocityBuffer, 128, 128);
	fb_SetupBloomBuffer(&local.bloomBuffer, &local.depthVelocityBuffer, 128, 128, 128, 128);

	fb_SetupQuad(&local.screenQuad);

	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
	rd_Viewport(width, height);
}

void rd_Shutdown(void)
{
	printf("Shutting down renderer...\n");

	sh_DestroyShader(&local.depthOnlyShader);
	sh_DestroyShader(&local.depthVelocityShader);
	sh_DestroyShader(&local.geometryShader);
	sh_DestroyShader(&local.lightingShader);
	sh_DestroyShader(&local.ssaoShader);
	sh_DestroyShader(&local.blurSingleChannelShader);
	sh_DestroyShader(&local.gaussianBlurSingleChannelShader);
	sh_DestroyShader(&local.shadowShader);
	sh_DestroyShader(&local.bloomShader);
	sh_DestroyShader(&local.ssrShader);
	sh_DestroyShader(&local.compositeShader);
	sh_DestroyShader(&local.tAAResolveMotionBlurShader);
	sh_DestroyShader(&local.postProcessShader);
	sh_DestroyShader(&local.debugSingleChannelShader);
	sh_DestroyShader(&local.debugDualChannelShader);
	sh_DestroyShader(&local.debugTripleChannelShader);
	sh_DestroyShader(&local.debugNormalsShader);

	fb_DestroyDepthVelocityBuffer(&local.depthVelocityBuffer);
	fb_DestroyGBuffer(&local.gBuffer);
	fb_DestroyColorBuffer(&local.colorBuffer);
	fb_DestroyColorBuffer(&local.frontBuffer);
	fb_DestroyColorBuffer(&local.backBuffer);
	fb_DestroyColorBuffer(&local.reflectionsBuffer);

	fb_DestroyAmbientOcclusionBuffer(&local.ssaoBuffer);
	fb_DestroyShadowsBuffer(&local.shadowsBuffer);
	fb_DestroyBloomBuffer(&local.bloomBuffer);

	fb_DestroyQuad(&local.screenQuad);
}

void rd_SetCustomAllocator(rdAlloc *alloc, rdFree *free)
{
	assert(alloc != NULL && free != NULL);

	mem.alloc = alloc;
	mem.free  = free;
}

void rd_Viewport(int width, int height)
{
	double aspect, fov;
	double zNear, zFar;
	double fw, fh;

	local.screenWidth  = width;
	local.screenHeight = height;

	aspect = (double) width / height;
	fov    = 35.45f * aspect;
	zNear  = 0.25f;
	zFar   = 30.0f;

	fh = tan(fov / 360.0f * RD_PI) * zNear;
	fw = fh * aspect;

	gl.Viewport(0, 0, width, height);

	mx_Frustum(&local.mProjection, -fw, fw, -fh, fh, zNear, zFar);
	
	local.mInvProjection = local.mProjection;
	mx_InvertFrustum(&local.mInvProjection);

	local.mProjectionJitter = local.mProjection;

	fb_DestroyDepthVelocityBuffer(&local.depthVelocityBuffer);
	fb_SetupDepthVelocityBuffer(&local.depthVelocityBuffer, width, height);

	fb_DestroyGBuffer(&local.gBuffer);
	fb_SetupGBuffer(&local.gBuffer, &local.depthVelocityBuffer, width, height);

	fb_DestroyColorBuffer(&local.colorBuffer);
	fb_SetupColorBuffer(&local.colorBuffer, width, height);

	fb_DestroyColorBuffer(&local.frontBuffer);
	fb_SetupColorBuffer(&local.frontBuffer, width, height);

	fb_DestroyColorBuffer(&local.backBuffer);
	fb_SetupColorBuffer(&local.backBuffer, width, height);

	fb_DestroyColorBuffer(&local.reflectionsBuffer);
	fb_SetupColorBuffer(&local.reflectionsBuffer, width / 2, height / 2);

	fb_DestroyAmbientOcclusionBuffer(&local.ssaoBuffer);
	fb_SetupAmbientOcclusionBuffer(&local.ssaoBuffer, width / 2, height / 2);

	fb_DestroyShadowsBuffer(&local.shadowsBuffer);
	fb_SetupShadowsBuffer(&local.shadowsBuffer, &local.depthVelocityBuffer, width, height);

	fb_DestroyBloomBuffer(&local.bloomBuffer);
	fb_SetupBloomBuffer(&local.bloomBuffer, &local.depthVelocityBuffer, width, height,
	                    width / 2,height / 2);
}

void rd_Clear(rdClearType clear)
{
	GLuint     framebuf;
	GLbitfield flags;

	switch (clear) {
	case RD_CLEAR_DEPTHVELOCITY:
		framebuf = local.depthVelocityBuffer.framebuf;
		flags = GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT;
		break;
	case RD_CLEAR_GBUFFER:
		framebuf = local.gBuffer.framebuf;
		flags = GL_COLOR_BUFFER_BIT;
		break;
	case RD_CLEAR_SHADOWS:
		framebuf = local.shadowsBuffer.framebuf;
		flags = GL_COLOR_BUFFER_BIT;
		break;
	case RD_CLEAR_BLOOM:
		framebuf = local.bloomBuffer.framebufRaw;
		flags = GL_COLOR_BUFFER_BIT;
		break;
	default:
		return;
	}

	gl.BindFramebuffer(GL_FRAMEBUFFER, framebuf);
	gl.Clear(flags);
}

void rd_ClearShadowMap(const rdShadowMap *sw)
{
	gl.BindFramebuffer(GL_FRAMEBUFFER, sw->framebuf);
	gl.Clear(GL_DEPTH_BUFFER_BIT);
}

void rd_Draw(rdDrawType draw, rdObject *obj)
{
	static int jitterIndex = 8;

	static rdVec2 currJitter = { 0.0f, 0.0f };
	static rdVec2 prevJitter = { 0.0f, 0.0f };

	int calcMVP = 0;

	rdMat4 mMVPCopy;
	rdMat4 mModelLightspace;
	GLuint framebuf;

	rdMat4 mModelView;
	rdMat3 mNormal;

	if (obj == NULL) {
		GLuint texture;

		if (draw == RD_DRAW_DEBUG_PREPASSDEPTH)
			texture = local.depthVelocityBuffer.depthTexture;
		else if (draw == RD_DRAW_DEBUG_NORMALS)
			texture = local.gBuffer.normalTexture;
		else if (draw == RD_DRAW_DEBUG_BLOOM)
			texture = local.bloomBuffer.bloomRawTexture;
		else if (draw == RD_DRAW_DEBUG_VELOCITY)
			texture = local.depthVelocityBuffer.velocityTexture;
		else if (draw == RD_DRAW_DEBUG_REFLECTIONS)
			texture = local.reflectionsBuffer.colorTexture;
		else
			return;

		gl.Disable(GL_DEPTH_TEST);

		gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
		gl.BindVertexArray(local.screenQuad.vertexArray);

		gl.ActiveTexture(GL_TEXTURE0);
		gl.BindTexture(GL_TEXTURE_2D, texture);

		if (draw == RD_DRAW_DEBUG_VELOCITY) {
			gl.UseProgram(local.debugDualChannelShader.shaderProgram);
			gl.Uniform1i(local.debugDualChannelShader.uniforms[0], 0);
		} else if (draw == RD_DRAW_DEBUG_NORMALS) {
			gl.UseProgram(local.debugNormalsShader.shaderProgram);
			gl.Uniform1i(local.debugNormalsShader.uniforms[0], 0);
		} else if (draw == RD_DRAW_DEBUG_REFLECTIONS) {
			gl.UseProgram(local.debugTripleChannelShader.shaderProgram);
			gl.Uniform1i(local.debugTripleChannelShader.uniforms[0], 0);
		} else {
			gl.UseProgram(local.debugSingleChannelShader.shaderProgram);
			gl.Uniform1i(local.debugSingleChannelShader.uniforms[0], 0);
		}

		gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
		gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

		gl.Enable(GL_DEPTH_TEST);

		return;
	}

	if (draw == RD_DRAW_DEBUG_SHADOWMAP) {
		GLuint texture;

		texture = obj->sm->depthTexture;
		assert(obj->sm != NULL);
		gl.Disable(GL_DEPTH_TEST);

		gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
		gl.BindVertexArray(local.screenQuad.vertexArray);

		gl.ActiveTexture(GL_TEXTURE0);
		gl.BindTexture(GL_TEXTURE_2D, texture);

		gl.UseProgram(local.debugSingleChannelShader.shaderProgram);
		gl.Uniform1i(local.debugSingleChannelShader.uniforms[0], 0);

		gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
		gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

		gl.Enable(GL_DEPTH_TEST);

		return;
	}

	if (draw == RD_DRAW_SHADOWMAP || draw == RD_DRAW_SHADOWS)
		assert(obj->sm != NULL);

	mMVPCopy = obj->mMVP;

	if (obj->mPrevMVP == NULL) {
		calcMVP = 1;
	}

	if (obj->update == 1) {
		mx_Identity(&obj->mModel);
		mx_Rotate(&obj->mModel, ma_ToRadians(obj->rotX), ma_ToRadians(obj->rotY),
		                        ma_ToRadians(obj->rotZ));
		mx_Scale(&obj->mModel, obj->scale, obj->scale, obj->scale);
		mx_Translate(&obj->mModel, obj->posX, obj->posY, -obj->posZ);

		obj->update = 0;
		calcMVP = 1;
	}

	if (local.defaultCamera.update) {
		cm_SyncViewMatrix(&local.defaultCamera);
		local.defaultCamera.update = 0;
		calcMVP = 1;
	}
	
	if (!cm_CheckCameraAttributes(&local.defaultCamera, &obj->lastCameraPosition,
	                             obj->lastCameraYaw, obj->lastCameraPitch))
		calcMVP = 1;

	if (draw == RD_DRAW_GBUFFER || draw == RD_DRAW_SHADOWS) {
		rdMat3 mInvModelView3;

		mx_MultiAB(&mModelView, &local.defaultCamera.mView, &obj->mModel);
		mInvModelView3 = mx_Mat3From4(&mModelView);
		mx_Invert3(&mInvModelView3);
		mNormal = mInvModelView3;
		mx_Transpose3(&mNormal);
	}

	if (local.renderState == RD_RENDERSTATE_FRESH) {
		float haltonX = 2.0f * ma_Halton(jitterIndex + 1, 2) - 1.0f;
		float haltonY = 2.0f * ma_Halton(jitterIndex + 1, 3) - 1.0f;

		float jitterX = (haltonX / local.screenWidth);
		float jitterY = (haltonY / local.screenHeight);

		local.mProjectionJitter = local.mProjection;

		local.mProjectionJitter.m[0][2] += jitterX;
		local.mProjectionJitter.m[1][2] += jitterY;

		prevJitter = currJitter;
		currJitter.x = jitterX;
		currJitter.y = jitterY;

		jitterIndex++;
		jitterIndex = jitterIndex % 8;
	}

	if (obj->jitterIndex != jitterIndex)
		calcMVP = 1;

	if (draw == RD_DRAW_SHADOWMAP || draw == RD_DRAW_SHADOWS)
		mx_MultiAB(&mModelLightspace, &obj->sm->mLightspace, &obj->mModel);

	if (calcMVP) {
		mx_MultiABC(&obj->mMVP, &local.mProjectionJitter, &local.defaultCamera.mView, &obj->mModel);
		obj->jitterIndex = jitterIndex;
	}

	switch (draw) {
	case RD_DRAW_DEPTHVELOCITY:
		framebuf = local.depthVelocityBuffer.framebuf;
		break;
	case RD_DRAW_SHADOWMAP:
		framebuf = obj->sm->framebuf;
		if (!obj->isIndexed)
			gl.CullFace(GL_FRONT);
		break;
	case RD_DRAW_GBUFFER:
		framebuf = local.gBuffer.framebuf;
		gl.DepthFunc(GL_EQUAL);
		gl.DepthMask(GL_FALSE);
		break;
	case RD_DRAW_SHADOWS:
		framebuf = local.shadowsBuffer.framebuf;
		gl.DepthFunc(GL_EQUAL);
		gl.DepthMask(GL_FALSE);
		break;
	case RD_DRAW_BLOOM:
		framebuf = local.bloomBuffer.framebufRaw;
		gl.DepthFunc(GL_EQUAL);
		gl.DepthMask(GL_FALSE);
		break;
	default:
		return;
	}

	gl.BindFramebuffer(GL_FRAMEBUFFER, framebuf);

	if (draw == RD_DRAW_SHADOWMAP)
		gl.Viewport(0, 0, obj->sm->pixWidth, obj->sm->pixHeight);

	switch (draw) {
	case RD_DRAW_DEPTHVELOCITY:
		gl.UseProgram(local.depthVelocityShader.shaderProgram);
		gl.UniformMatrix4fv(local.depthVelocityShader.uniforms[0], 1, GL_TRUE, &obj->mMVP.m[0][0]);
		if (obj->mPrevMVP) {
			gl.UniformMatrix4fv(local.depthVelocityShader.uniforms[1], 1, GL_TRUE,
			                    &obj->mPrevMVP->m[0][0]);
		} else {
			gl.UniformMatrix4fv(local.depthVelocityShader.uniforms[1], 1, GL_TRUE,
			                    &obj->mMVP.m[0][0]);
		}
		gl.Uniform2fv(local.depthVelocityShader.uniforms[2], 1, &currJitter.x);
		gl.Uniform2fv(local.depthVelocityShader.uniforms[3], 1, &prevJitter.x);
		break;
	case RD_DRAW_SHADOWMAP:
		gl.UseProgram(local.depthOnlyShader.shaderProgram);
		gl.UniformMatrix4fv(local.depthOnlyShader.uniforms[0], 1, GL_TRUE,
		                    &mModelLightspace.m[0][0]);
		break;
	case RD_DRAW_GBUFFER:
		gl.UseProgram(local.geometryShader.shaderProgram);
		gl.UniformMatrix4fv(local.geometryShader.uniforms[0], 1, GL_TRUE, &mModelView.m[0][0]);
		gl.UniformMatrix4fv(local.geometryShader.uniforms[1], 1, GL_TRUE, &obj->mMVP.m[0][0]);
		gl.UniformMatrix3fv(local.geometryShader.uniforms[2], 1, GL_TRUE, &mNormal.m[0][0]);
		gl.Uniform1i(local.geometryShader.uniforms[3], obj->materialType == RD_MATERIAL_PAINTJOB);
		gl.Uniform1i(local.geometryShader.uniforms[4], obj->materialID);
		break;
	case RD_DRAW_SHADOWS:
	{
		rdVec4 tmp, lightPosViewspace;

		tmp.x = local.lights[obj->sm->originLightIndex].x;
		tmp.y = local.lights[obj->sm->originLightIndex].y;
		tmp.z = local.lights[obj->sm->originLightIndex].z;
		tmp.w = 1.0f;

		lightPosViewspace = mx_MultiVector4(&local.defaultCamera.mView, &tmp);

		gl.ActiveTexture(GL_TEXTURE0);
		gl.BindTexture(GL_TEXTURE_2D, obj->sm->depthTexture);

		gl.UseProgram(local.shadowShader.shaderProgram);
		gl.UniformMatrix4fv(local.shadowShader.uniforms[0], 1, GL_TRUE, &obj->mMVP.m[0][0]);
		gl.UniformMatrix3fv(local.shadowShader.uniforms[1], 1, GL_TRUE, &mNormal.m[0][0]);
		gl.UniformMatrix4fv(local.shadowShader.uniforms[2], 1, GL_TRUE, &mModelView.m[0][0]);
		gl.UniformMatrix4fv(local.shadowShader.uniforms[3], 1, GL_TRUE, &mModelLightspace.m[0][0]);
		gl.Uniform1i(local.shadowShader.uniforms[4], 0);
		gl.Uniform3fv(local.shadowShader.uniforms[5], 1, &lightPosViewspace.x);
		break;
	}
	case RD_DRAW_BLOOM:
		gl.UseProgram(local.bloomShader.shaderProgram);
		gl.UniformMatrix4fv(local.bloomShader.uniforms[0], 1, GL_TRUE, &obj->mMVP.m[0][0]);
		break;
	default:
		return;
	}

	gl.BindVertexArray(obj->vertexArray);

	if (obj->objectType == RD_OBJECT_INTERIOR)
		gl.Disable(GL_CULL_FACE);

	if (obj->isIndexed) {
		gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indexBuffer);
		gl.DrawElements(GL_TRIANGLES, obj->numIndices, GL_UNSIGNED_SHORT, NULL);
	} else {
		gl.DrawArrays(GL_TRIANGLES, 0, obj->numVertices);
	}
	
	if (obj->objectType == RD_OBJECT_INTERIOR)
		gl.Enable(GL_CULL_FACE);

	if (draw == RD_DRAW_GBUFFER || draw == RD_DRAW_SHADOWS || draw == RD_DRAW_BLOOM) {
		gl.DepthFunc(GL_LESS);
		gl.DepthMask(GL_TRUE);
	}
	else if (draw == RD_DRAW_SHADOWMAP)
		gl.CullFace(GL_BACK);

	if (draw == RD_DRAW_SHADOWMAP)
		gl.Viewport(0, 0, local.screenWidth, local.screenHeight);

	if (draw == RD_DRAW_DEPTHVELOCITY) {
		obj->_mPrevMVP = mMVPCopy;
		obj->mPrevMVP = &obj->_mPrevMVP;
	}

	obj->lastCameraPosition.x = local.defaultCamera.x;
	obj->lastCameraPosition.y = local.defaultCamera.y;
	obj->lastCameraPosition.z = local.defaultCamera.z;

	obj->lastCameraYaw   = local.defaultCamera.yaw;
	obj->lastCameraPitch = local.defaultCamera.pitch;

	local.renderState = RD_RENDERSTATE_PARTIAL;
}

void rd_Frame(void)
{
	typedef struct rdFrameStage rdFrameStage;
	struct rdFrameStage
	{
		rdVec2 aoResolution;

		int numLights;

		rdVec3 lightPositions[64];
		rdVec3 lightColors[64];
		rdVec3 lightProperties[64];
		rdVec3 materialColors[64];
		rdVec3 materialProperties[64];

		rdVec3 viewspaceUp;

		float reflectanceProperties[64];

		float  randomInput;
		rdVec3 lensFlareLightPos;
		int    lensFlareEnabled;
		rdVec2 resolution;

	};

	rdFrameStage stage;

	static int frontOrBackBuffer = 0;

	/* Set stage variables */

	stage.aoResolution = vc_Vec2(local.ssaoBuffer.width, local.ssaoBuffer.height);

	stage.numLights = 0;

	for (int i = 0; i < 64; i++) {
		stage.lightPositions[i]  = vc_Vec3(0.0f, 0.0f, 0.0f);
		stage.lightColors[i]     = vc_Vec3(0.0f, 0.0f, 0.0f);
		stage.lightProperties[i] = vc_Vec3(0.0f, 0.0f, 0.0f);
	}

	for (int i = 0; i < 64; i++) {
		rdLight *l;

		l = &local.lights[i];

		if (l->enabled) {
			int n = stage.numLights;

			rdVec4 tmp, tmp2;

			tmp = vc_Vec4(l->x, l->y, l->z, 1.0f);
			tmp2 = mx_MultiVector4(&local.defaultCamera.mView, &tmp);

			stage.lightPositions[n].x = tmp2.x;
			stage.lightPositions[n].y = tmp2.y;
			stage.lightPositions[n].z = tmp2.z;

			stage.lightColors[n].x = l->red;
			stage.lightColors[n].y = l->green;
			stage.lightColors[n].z = l->blue;

			stage.lightProperties[n].x = l->intensity;
			stage.lightProperties[n].y = l->cutoffRadius;
			stage.lightProperties[n].z = l->upward;

			stage.numLights++;
		}
	}

	for (int i = 0; i < 64; i++) {
		rdMaterial *m;
		rdVec3 *c, *p;

		m = &local.materials[i];
		c = &stage.materialColors[i];
		p = &stage.materialProperties[i];

		c->x = m->red;
		c->y = m->green;
		c->z = m->blue;

		p->x = m->metalness;
		p->y = m->roughness;
		p->z = m->ambient;
	}

	{
		rdMat3 mNormal;

		rdVec3 up = vc_Vec3(0.0, 1.0f, 0.0f);

		mNormal = mx_Mat3From4(&local.defaultCamera.mView);
		mx_Invert3(&mNormal);
		mx_Transpose3(&mNormal);

		stage.viewspaceUp = mx_MultiVector3(&mNormal, &up);
	}

	for (int i = 0; i < 64; i++) {
		stage.reflectanceProperties[i] = local.materials[i].reflectance;
	}

	stage.resolution.x = local.screenWidth;
	stage.resolution.y = local.screenHeight;

	stage.randomInput       = ma_Random(0.0f, 100.0f);
	stage.lensFlareEnabled  = 0;
	stage.lensFlareLightPos = vc_Vec3(0.0f, 0.0f, 0.0f);


	/* Begin assembling final frame */

	gl.Disable(GL_DEPTH_TEST);

	/* SSAO pass */

	gl.Viewport(0, 0, local.ssaoBuffer.width, local.ssaoBuffer.height);

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.ssaoBuffer.framebufRaw);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.ssaoShader.shaderProgram);
	gl.Uniform1i(local.ssaoShader.uniforms[0], 3);
	gl.Uniform1i(local.ssaoShader.uniforms[1], 1);
	gl.Uniform1i(local.ssaoShader.uniforms[2], 2);
	gl.UniformMatrix4fv(local.ssaoShader.uniforms[3], 1, GL_TRUE, &local.mProjection.m[0][0]);
	gl.UniformMatrix4fv(local.ssaoShader.uniforms[4], 1, GL_TRUE, &local.mInvProjection.m[0][0]);
	gl.Uniform3fv(local.ssaoShader.uniforms[5], 64, &local.ssaoBuffer.kernel[0].x);
	gl.Uniform2fv(local.ssaoShader.uniforms[6], 1, &stage.aoResolution.x);

	gl.ActiveTexture(GL_TEXTURE1);
	gl.BindTexture(GL_TEXTURE_2D, local.gBuffer.normalTexture);
	gl.ActiveTexture(GL_TEXTURE2);
	gl.BindTexture(GL_TEXTURE_2D, local.ssaoBuffer.noiseTexture);
	gl.ActiveTexture(GL_TEXTURE3);
	gl.BindTexture(GL_TEXTURE_2D, local.depthVelocityBuffer.depthTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	/* SSAO blur */

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.ssaoBuffer.framebufBlur);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.blurSingleChannelShader.shaderProgram);
	gl.Uniform1i(local.blurSingleChannelShader.uniforms[0], 0);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.ssaoBuffer.occlusionRawTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.Viewport(0, 0, local.screenWidth, local.screenHeight);

	/* Bloom blur*/

	gl.Viewport(0, 0, local.bloomBuffer.blurWidth, local.bloomBuffer.blurHeight);

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.bloomBuffer.framebufBlur1);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.gaussianBlurSingleChannelShader.shaderProgram);
	gl.Uniform1i(local.gaussianBlurSingleChannelShader.uniforms[0], 0);
	gl.Uniform1i(local.gaussianBlurSingleChannelShader.uniforms[1], 0);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.bloomBuffer.bloomRawTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.bloomBuffer.framebufBlur2);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.gaussianBlurSingleChannelShader.shaderProgram);
	gl.Uniform1i(local.gaussianBlurSingleChannelShader.uniforms[0], 0);
	gl.Uniform1i(local.gaussianBlurSingleChannelShader.uniforms[1], 1);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.bloomBuffer.bloomRawTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.bloomBuffer.framebufBlur1);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.gaussianBlurSingleChannelShader.shaderProgram);
	gl.Uniform1i(local.gaussianBlurSingleChannelShader.uniforms[0], 0);
	gl.Uniform1i(local.gaussianBlurSingleChannelShader.uniforms[1], 0);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.bloomBuffer.bloomBlur2Texture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.Viewport(0, 0, local.screenWidth, local.screenHeight);

	/* Lighting */

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.colorBuffer.framebuf);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.lightingShader.shaderProgram);
	gl.Uniform1i(local.lightingShader.uniforms[0], 5);
	gl.Uniform1i(local.lightingShader.uniforms[1], 0);
	gl.Uniform1i(local.lightingShader.uniforms[2], 2);
	gl.Uniform1i(local.lightingShader.uniforms[3], 3);
	gl.Uniform1i(local.lightingShader.uniforms[4], 4);
	gl.Uniform1i(local.lightingShader.uniforms[5], 6);
	gl.Uniform1i(local.lightingShader.uniforms[6], 7);
	gl.UniformMatrix4fv(local.lightingShader.uniforms[7], 1, GL_TRUE,
	                    &local.mInvProjection.m[0][0]);
	gl.Uniform3fv(local.lightingShader.uniforms[8], 1, &stage.viewspaceUp.x);
	gl.Uniform3fv(local.lightingShader.uniforms[9], 64, &stage.materialColors[0].x);
	gl.Uniform3fv(local.lightingShader.uniforms[10], 64, &stage.materialProperties[0].x);

	gl.Uniform1i(local.lightingShader.uniforms[11], stage.numLights);
	gl.Uniform3fv(local.lightingShader.uniforms[12], 64, &stage.lightPositions[0].x);
	gl.Uniform3fv(local.lightingShader.uniforms[13], 64, &stage.lightColors[0].x);
	gl.Uniform3fv(local.lightingShader.uniforms[14], 64, &stage.lightProperties[0].x);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.gBuffer.materialIDTexture);
	gl.ActiveTexture(GL_TEXTURE2);
	gl.BindTexture(GL_TEXTURE_2D, local.gBuffer.normalTexture);
	gl.ActiveTexture(GL_TEXTURE3);
	gl.BindTexture(GL_TEXTURE_2D, local.ssaoBuffer.occlusionBlurTexture);
	gl.ActiveTexture(GL_TEXTURE4);
	gl.BindTexture(GL_TEXTURE_2D, local.shadowsBuffer.shadowsTexture);
	gl.ActiveTexture(GL_TEXTURE5);
	gl.BindTexture(GL_TEXTURE_2D, local.depthVelocityBuffer.depthTexture);
	gl.ActiveTexture(GL_TEXTURE6);
	gl.BindTexture(GL_TEXTURE_2D, local.bloomBuffer.bloomRawTexture);
	gl.ActiveTexture(GL_TEXTURE7);
	gl.BindTexture(GL_TEXTURE_2D, local.bloomBuffer.bloomBlur1Texture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	/* SSR pass */

	gl.Viewport(0, 0, local.reflectionsBuffer.pixWidth, local.reflectionsBuffer.pixHeight);

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.reflectionsBuffer.framebuf);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.ssrShader.shaderProgram);
	gl.UniformMatrix4fv(local.ssrShader.uniforms[0], 1, GL_TRUE, &local.mProjection.m[0][0]);
	gl.UniformMatrix4fv(local.ssrShader.uniforms[1], 1, GL_TRUE, &local.mInvProjection.m[0][0]);
	gl.Uniform1i(local.ssrShader.uniforms[2], 0);
	gl.Uniform1i(local.ssrShader.uniforms[4], 1);
	gl.Uniform1i(local.ssrShader.uniforms[5], 2);
	gl.Uniform1i(local.ssrShader.uniforms[3], 3);
	gl.Uniform1fv(local.ssrShader.uniforms[6], 64, &stage.reflectanceProperties[0]);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.depthVelocityBuffer.depthTexture);
	gl.ActiveTexture(GL_TEXTURE1);
	gl.BindTexture(GL_TEXTURE_2D, local.gBuffer.normalTexture);
	gl.ActiveTexture(GL_TEXTURE2);
	gl.BindTexture(GL_TEXTURE_2D, local.colorBuffer.colorTexture);
	gl.ActiveTexture(GL_TEXTURE3);
	gl.BindTexture(GL_TEXTURE_2D, local.gBuffer.materialIDTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.Viewport(0, 0, local.screenWidth, local.screenHeight);

	/* Composite reflections */

	if (frontOrBackBuffer == 0)
		gl.BindFramebuffer(GL_FRAMEBUFFER, local.frontBuffer.framebuf);
	else
		gl.BindFramebuffer(GL_FRAMEBUFFER, local.backBuffer.framebuf);

	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.compositeShader.shaderProgram);
	gl.Uniform1i(local.compositeShader.uniforms[0], 0);
	gl.Uniform1i(local.compositeShader.uniforms[1], 1);
	gl.Uniform1i(local.compositeShader.uniforms[2], 2);
	gl.Uniform1fv(local.compositeShader.uniforms[3], 64, &stage.reflectanceProperties[0]);
	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.colorBuffer.colorTexture);
	gl.ActiveTexture(GL_TEXTURE1);
	gl.BindTexture(GL_TEXTURE_2D, local.reflectionsBuffer.colorTexture);
	gl.ActiveTexture(GL_TEXTURE2);
	gl.BindTexture(GL_TEXTURE_2D, local.gBuffer.materialIDTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	/* TAA resolve + motion blur */

	gl.BindFramebuffer(GL_FRAMEBUFFER, local.colorBuffer.framebuf);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.tAAResolveMotionBlurShader.shaderProgram);
	gl.Uniform1i(local.tAAResolveMotionBlurShader.uniforms[0], 0);
	gl.Uniform1i(local.tAAResolveMotionBlurShader.uniforms[1], 1);
	gl.Uniform1i(local.tAAResolveMotionBlurShader.uniforms[2], 2);
	gl.Uniform1i(local.tAAResolveMotionBlurShader.uniforms[3], 3);
	gl.Uniform2fv(local.tAAResolveMotionBlurShader.uniforms[4], 1, &stage.resolution.x);

	gl.ActiveTexture(GL_TEXTURE0);
	if (frontOrBackBuffer == 0)
		gl.BindTexture(GL_TEXTURE_2D, local.frontBuffer.colorTexture);
	else
		gl.BindTexture(GL_TEXTURE_2D, local.backBuffer.colorTexture);
	gl.ActiveTexture(GL_TEXTURE1);
	if (frontOrBackBuffer == 0)
		gl.BindTexture(GL_TEXTURE_2D, local.backBuffer.colorTexture);
	else
		gl.BindTexture(GL_TEXTURE_2D, local.frontBuffer.colorTexture);
	gl.ActiveTexture(GL_TEXTURE2);
	gl.BindTexture(GL_TEXTURE_2D, local.depthVelocityBuffer.depthTexture);
	gl.ActiveTexture(GL_TEXTURE3);
	gl.BindTexture(GL_TEXTURE_2D, local.depthVelocityBuffer.velocityTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
	gl.BindVertexArray(local.screenQuad.vertexArray);

	gl.UseProgram(local.postProcessShader.shaderProgram);
	gl.Uniform1i(local.postProcessShader.uniforms[0], 0);
	gl.Uniform1f(local.postProcessShader.uniforms[1], stage.randomInput);
	gl.Uniform2fv(local.postProcessShader.uniforms[2], 1, &stage.resolution.x);
	gl.Uniform1i(local.postProcessShader.uniforms[3], stage.lensFlareEnabled);
	gl.Uniform2fv(local.postProcessShader.uniforms[4], 1, &stage.lensFlareLightPos.x);

	gl.ActiveTexture(GL_TEXTURE0);
	gl.BindTexture(GL_TEXTURE_2D, local.colorBuffer.colorTexture);

	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, local.screenQuad.indexBuffer);
	gl.DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

	gl.Enable(GL_DEPTH_TEST);

	local.renderState = RD_RENDERSTATE_FRESH;
	frontOrBackBuffer = frontOrBackBuffer == 1;
}

void rd_SetLight(int index, float x, float y, float z, float red, float green, float blue,
                 float intensity, float cutoffRadius, float upward)
{
	rdLight *l;

	assert(index >= 0 && index <= 63);

	l = &local.lights[index];

	l->x = x;
	l->y = y;
	l->z = z;

	l->red   = ma_Clamp(red, 0.0f, 1.0f);
	l->green = ma_Clamp(green, 0.0f, 1.0f);
	l->blue  = ma_Clamp(blue, 0.0f, 1.0f);

	l->intensity    = ma_Clamp(intensity, 0.01f, 1000.0f);
	l->cutoffRadius = ma_Clamp(cutoffRadius, 0.01f, 1000.0f);
	l->upward       = ma_Clamp(upward, 0.0f, 1.0f);
}

void rd_EnableLight(int index)
{
	assert(index <= 64);

	local.lights[index].enabled = 1;
}

void rd_DisableLight(int index)
{
	assert(index <= 64);

	local.lights[index].enabled = 0;
}

void rd_SetMaterial(int index, float red, float green, float blue, float metalness, float roughness,
                    float ambient, float reflectance)
{
	assert(index >= 0 && index <= 63);

	rdMaterial *m = &local.materials[index];

	m->red   = ma_Clamp(red, 0.0f, 1.0f);
	m->green = ma_Clamp(green, 0.0f, 1.0f);
	m->blue  = ma_Clamp(blue, 0.0f, 1.0f);

	m->metalness = ma_Clamp(metalness, 0.0f, 1.0f);
	m->roughness = ma_Clamp(roughness, 0.0f, 1.0f);

	m->ambient     = ma_Clamp(ambient, 0.0f, 1.0f);
	m->reflectance = ma_Clamp(reflectance, 0.0f, 1.0f);
}

rdShadowMap *rd_CreateShadowMap(int originLightIndex, int pixWidth, int pixHeight, float targetX,
                                float targetY, float targetZ, float spanWidth, float spanHeight,
                                float zNear, float zFar)
{
	const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	rdShadowMap *sm;

	rdMat4 mLightView;
	rdMat4 mLightProjection;

	rdVec3 lightPosition;
	rdVec3 target;
	rdVec3 up;

	sm = mem.alloc(sizeof (*sm));
	if (sm == NULL)
		return NULL;

	sm->pixWidth = pixWidth;
	sm->pixHeight = pixHeight;

	gl.GenFramebuffers(1, &sm->framebuf);
	gl.BindFramebuffer(GL_FRAMEBUFFER, sm->framebuf);

	gl.GenTextures(1, &sm->depthTexture);
	gl.BindTexture(GL_TEXTURE_2D, sm->depthTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, pixWidth, pixHeight, 0, GL_DEPTH_COMPONENT,
	              GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	gl.TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sm->depthTexture,
	                        0);

	gl.DrawBuffer(GL_NONE);
	gl.ReadBuffer(GL_NONE);

	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);

	lightPosition.x = local.lights[originLightIndex].x;
	lightPosition.y = local.lights[originLightIndex].y;
	lightPosition.z = local.lights[originLightIndex].z;

	target = vc_Vec3(targetX, targetY, targetZ);
	up     = vc_Vec3(0.0f, 1.0f, 0.0f);

	mx_LookAt(&mLightView, &lightPosition, &target, &up);
	mx_Frustum(&mLightProjection, -(spanWidth / 2.0f), spanWidth / 2.0f, -(spanHeight / 2.0f),
	           spanHeight / 2.0f, zNear, zFar);
	mx_MultiAB(&sm->mLightspace, &mLightProjection, &mLightView);

	sm->originLightIndex = originLightIndex;
	sm->numObjectsAttached = 0;

	return sm;
}

void rd_DestroyShadowMap(rdShadowMap *sm)
{
	assert(sm->numObjectsAttached == 0);

	gl.DeleteTextures(1, &sm->depthTexture);
	gl.DeleteFramebuffers(1, &sm->framebuf);

	mem.free(sm);
}

void rd_AttachShadowMap(rdObject *obj, rdShadowMap *sm)
{
	assert(obj->sm == NULL);

	obj->sm = sm;
	sm->numObjectsAttached++;
}

rdObject *rd_CreateObject(int numVertices, const rdVertex *vertices, int numIndices,
	                      const rdIndex *indices, rdObjectType objectType,
	                      rdMaterialType materialType)
{
	rdVec3    normals[numVertices];
	rdObject *obj;

	obj = mem.alloc(sizeof (*obj));
	if (obj == NULL)
		return NULL;

	obj->parent    = NULL;
	obj->numClones = 0;

	obj->objectType   = objectType;
	obj->materialType = materialType;
	obj->materialID   = 0;

	obj->numVertices = numVertices;
	obj->numIndices  = numIndices;

	obj->posX  = obj->posY = obj->posZ = 0.0f;
	obj->scale = 1.0f;
	obj->rotX  = obj->rotY = obj->rotZ = 0.0f;

	mx_Identity(&obj->mModel);
	obj->update = 0;
	if (indices)
		obj->isIndexed = 1;
	else
		obj->isIndexed = 0;

	obj->vertexBuffer = 0;
	obj->normalBuffer = 0;
	obj->indexBuffer  = 0;
	obj->vertexArray  = 0;

	mx_Identity(&obj->mMVP);

	obj->mPrevMVP = NULL;
	mx_Identity(&obj->_mPrevMVP);

	if (indices)
		me_GenerateNormalsIndexed(normals, numVertices, vertices, numIndices, indices);
	else
		me_GenerateNormalsNonIndexed(normals, numVertices, vertices);

	if (objectType == RD_OBJECT_INTERIOR)
		me_InvertNormals(normals, numVertices);

	gl.GenBuffers(1, &obj->vertexBuffer);
	gl.BindBuffer(GL_ARRAY_BUFFER, obj->vertexBuffer);
	gl.BufferData(GL_ARRAY_BUFFER, numVertices * 3 * sizeof (float), vertices, GL_STATIC_DRAW);

	gl.GenBuffers(1, &obj->normalBuffer);
	gl.BindBuffer(GL_ARRAY_BUFFER, obj->normalBuffer);
	gl.BufferData(GL_ARRAY_BUFFER, numVertices * 3 * sizeof (float), normals, GL_STATIC_DRAW);

	if (indices != NULL) {
		gl.GenBuffers(1, &obj->indexBuffer);
		gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, obj->indexBuffer);
		gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, numIndices * sizeof (unsigned short), indices,
		              GL_STATIC_DRAW);
	}

	gl.GenVertexArrays(1, &obj->vertexArray);
	gl.BindVertexArray(obj->vertexArray);
	gl.BindBuffer(GL_ARRAY_BUFFER, obj->vertexBuffer);
	gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	gl.BindBuffer(GL_ARRAY_BUFFER, obj->normalBuffer);
	gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	gl.EnableVertexAttribArray(0);
	gl.EnableVertexAttribArray(1);

	obj->lastCameraPosition = vc_Vec3(0.0f, 0.0f, 0.0f);
	obj->lastCameraYaw = 0.0f;
	obj->lastCameraPitch = 0.0f;

	obj->jitterIndex = -1;

	obj->sm = NULL;

	return obj;
}

void rd_DestroyObject(rdObject *obj)
{
	assert(obj->numClones == 0);

	gl.DeleteVertexArrays(1, &obj->vertexArray);
	gl.DeleteBuffers(1, &obj->vertexBuffer);
	gl.DeleteBuffers(1, &obj->normalBuffer);

	if (obj->isIndexed)
		gl.DeleteBuffers(1, &obj->indexBuffer);

	if (obj->sm)
		obj->sm->numObjectsAttached--;

	mem.free(obj);
}

rdObject *rd_CloneObject(rdObject *original)
{
	rdObject *obj;

	obj = mem.alloc(sizeof (*obj));
	if (obj == NULL)
		return NULL;

	obj->parent    = original;
	obj->numClones = 0;

	obj->objectType   = original->objectType;
	obj->materialType = original->materialType;
	obj->materialID   = original->materialID;

	obj->numVertices = original->numVertices;
	obj->numIndices  = original->numIndices;

	obj->posX = original->posX;
	obj->posY = original->posY;
	obj->posZ = original->posZ;

	obj->scale = original->scale;

	obj->rotX = original->rotX;
	obj->rotY = original->rotY;
	obj->rotZ = original->rotZ;

	obj->mModel = original->mModel;
	obj->update = original->update;

	obj->isIndexed = original->isIndexed;

	obj->vertexBuffer = original->vertexBuffer;
	obj->normalBuffer = original->normalBuffer;
	obj->indexBuffer  = original->indexBuffer;
	obj->vertexArray  = original->vertexArray;

	obj->mMVP      = original->mMVP;
	obj->_mPrevMVP = original->_mPrevMVP;

	obj->mPrevMVP = &obj->_mPrevMVP;

	obj->lastCameraPosition = original->lastCameraPosition;
	obj->lastCameraYaw = original->lastCameraYaw;
	obj->lastCameraPitch = original->lastCameraPitch;

	obj->jitterIndex = original->jitterIndex;

	obj->sm = original->sm;

	if (obj->sm != NULL)
		obj->sm->numObjectsAttached++;

	return obj;
}

void rd_SetObjectMaterial(rdObject *obj, int materialID)
{
	obj->materialID = materialID;
}

void rd_ResetObject(rdObject *obj)
{
	obj->update = 1;

	obj->posX  = obj->posY = obj->posZ = 0.0f;
	obj->scale = 1.0f;
	obj->rotX  = obj->rotY = obj->rotZ = 0.0f;
}

void rd_PositionObject(rdObject *obj, float x, float y, float z)
{
	obj->update = 1;

	obj->posX = x;
	obj->posY = y;
	obj->posZ = z;
}

void rd_MoveObject(rdObject *obj, float x, float y, float z)
{
	obj->update = 1;

	obj->posX += x;
	obj->posY += y;
	obj->posZ += z;
}

void rd_ScaleObject(rdObject *obj, float scale)
{
	obj->update = 1;

	obj->scale = scale;
}

void rd_OrientObject(rdObject *obj, float x, float y, float z)
{
	obj->update = 1;

	obj->rotX = ma_WrapAngle(x);
	obj->rotY = ma_WrapAngle(y);
	obj->rotZ = ma_WrapAngle(z);
}

void rd_RotateObject(rdObject *obj, float x, float y, float z)
{
	obj->update = 1;

	obj->rotX = ma_WrapAngle(obj->rotX + x);
	obj->rotY = ma_WrapAngle(obj->rotY + y);
	obj->rotZ = ma_WrapAngle(obj->rotZ + z);
}

void rd_ResetDefaultCamera(void)
{
	local.defaultCamera.update = 1;

	local.defaultCamera.x = local.defaultCamera.y = local.defaultCamera.z = 0.0f;
	local.defaultCamera.yaw   = 0.0f;
	local.defaultCamera.pitch = 0.0f;
}

void rd_PositionDefaultCamera(float x, float y, float z)
{
	local.defaultCamera.update = 1;

	local.defaultCamera.x = x;
	local.defaultCamera.y = y;
	local.defaultCamera.z = z;
}

void rd_MoveDefaultCamera(float x, float y, float z)
{
	local.defaultCamera.update = 1;

	local.defaultCamera.x += x;
	local.defaultCamera.y += y;
	local.defaultCamera.z += z;
}

void rd_MoveDefaultCameraFacingDirection(float right, float forward)
{
	float turnX, turnZ;
	float yaw = local.defaultCamera.yaw - 90.0f;

	local.defaultCamera.update = 1;

	turnX = cosf(ma_ToRadians(yaw));
	turnZ = sinf(ma_ToRadians(yaw));

	local.defaultCamera.x +=  forward * turnX - right * turnZ;
	local.defaultCamera.z += -forward * turnZ - right * turnX;
}

void rd_OrientDefaultCamera(float yaw, float pitch)
{
	local.defaultCamera.update = 1;

	local.defaultCamera.yaw   = ma_WrapAngle(yaw);
	local.defaultCamera.pitch = ma_WrapAngle(pitch);
}

void rd_RotateDefaultCamera(float yaw, float pitch)
{
	local.defaultCamera.update = 1;

	local.defaultCamera.yaw   = ma_WrapAngle(local.defaultCamera.yaw + yaw);
	local.defaultCamera.pitch = ma_WrapAngle(local.defaultCamera.pitch + pitch);
	local.defaultCamera.pitch = ma_Clamp(local.defaultCamera.pitch, -89.9999f, 89.9999f);
}

void rd_GetDefaultCameraPosition(float *x, float *y, float *z)
{
	if (x)
		*x = local.defaultCamera.x;
	if (y)
		*y = local.defaultCamera.y;
	if (z)
		*z = local.defaultCamera.z;
}

void rd_GetDefaultCameraOrientation(float *yaw, float *pitch)
{
	if (yaw)
		*yaw = local.defaultCamera.yaw;
	if (pitch)
		*pitch = local.defaultCamera.pitch;
}

static void cm_ResetCamera(rdCamera *cam)
{
	cam->update = 1;

	cam->x = cam->y = cam->z = 0.0f;
	cam->yaw   = 0.0f;
	cam->pitch = 0.0f;
}

static void cm_SyncViewMatrix(rdCamera *cam)
{
	float       yawRadians, pitchRadians;
	rdVec3      up, position, center, turn;

	yawRadians   = ma_ToRadians(cam->yaw - 90.0f);
	pitchRadians = ma_ToRadians(cam->pitch);

	up       = vc_Vec3(0.0f, 1.0f, 0.0f);
	position = vc_Vec3(cam->x, cam->y, -cam->z);

	turn = vc_Vec3(cosf(pitchRadians) * cosf(yawRadians),
	               sinf(pitchRadians),
	               cosf(pitchRadians) * sinf(yawRadians));

	center = vc_Add(&position, &turn);

	mx_LookAt(&cam->mView, &position, &center, &up);
}

static int cm_CheckCameraAttributes(const rdCamera *cam, const rdVec3 *pos, float yaw, float pitch)
{
	if (cam->yaw != yaw || cam->pitch != pitch)
		return 0;
	if (cam->x != pos->x || cam->y != pos->y || cam->z != pos->z)
		return 0;
	return 1;
}

static void fb_SetupQuad(rdQuad *quad)
{
	const float vertices[]  = {-1.0f,  1.0f, 1.0f,  1.0f,
		                       -1.0f, -1.0f, 1.0f, -1.0f };

	const float uv[]        = { 0.0f, 1.0f, 1.0f, 1.0f,
		                        0.0f, 0.0f, 1.0f, 0.0f };

	const unsigned short indices[] = { 1, 0, 2, 2, 3, 1 };

	gl.GenBuffers(1, &quad->vertexBuffer);
	gl.BindBuffer(GL_ARRAY_BUFFER, quad->vertexBuffer);
	gl.BufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof (float), vertices, GL_STATIC_DRAW);

	gl.GenBuffers(1, &quad->uvBuffer);
	gl.BindBuffer(GL_ARRAY_BUFFER, quad->uvBuffer);
	gl.BufferData(GL_ARRAY_BUFFER, 4 * 2 * sizeof (float), uv, GL_STATIC_DRAW);

	gl.GenBuffers(1, &quad->indexBuffer);
	gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad->indexBuffer);
	gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * sizeof (unsigned short), indices, GL_STATIC_DRAW);

	gl.GenVertexArrays(1, &quad->vertexArray);
	gl.BindVertexArray(quad->vertexArray);
	gl.BindBuffer(GL_ARRAY_BUFFER, quad->vertexBuffer);
	gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	gl.BindBuffer(GL_ARRAY_BUFFER, quad->uvBuffer);
	gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	gl.EnableVertexAttribArray(0);
	gl.EnableVertexAttribArray(1);
}

static void fb_DestroyQuad(rdQuad *quad)
{
	gl.DeleteVertexArrays(1, &quad->vertexArray);
	gl.DeleteBuffers(1, &quad->vertexBuffer);
	gl.DeleteBuffers(1, &quad->uvBuffer);
	gl.DeleteBuffers(1, &quad->indexBuffer);
}

static void fb_SetupDepthVelocityBuffer(rdDepthVelocityBuffer *depthVelocityBuffer, int screenWidth,
                                        int screenHeight)
{
	const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };

	gl.GenFramebuffers(1, &depthVelocityBuffer->framebuf);
	gl.BindFramebuffer(GL_FRAMEBUFFER, depthVelocityBuffer->framebuf);

	gl.GenTextures(1, &depthVelocityBuffer->depthTexture);
	gl.BindTexture(GL_TEXTURE_2D, depthVelocityBuffer->depthTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, screenWidth, screenHeight, 0,
	              GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	gl.TexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
	                        depthVelocityBuffer->depthTexture, 0);

	gl.GenTextures(1, &depthVelocityBuffer->velocityTexture);
	gl.BindTexture(GL_TEXTURE_2D, depthVelocityBuffer->velocityTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, depthVelocityBuffer->velocityTexture, 0);

	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void fb_DestroyDepthVelocityBuffer(rdDepthVelocityBuffer *depthVelocityBuffer)
{
	gl.DeleteTextures(1, &depthVelocityBuffer->depthTexture);
	gl.DeleteTextures(1, &depthVelocityBuffer->velocityTexture);
	gl.DeleteFramebuffers(1, &depthVelocityBuffer->framebuf);
}

static void fb_SetupGBuffer(rdGBuffer *gBuffer, const rdDepthVelocityBuffer *depthVelocityBuffer,
                            int screenWidth, int screenHeight)
{
	const GLenum bufferAttachments[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

	gl.GenFramebuffers(1, &gBuffer->framebuf);
	gl.BindFramebuffer(GL_FRAMEBUFFER, gBuffer->framebuf);

	gl.GenTextures(1, &gBuffer->materialIDTexture);
	gl.BindTexture(GL_TEXTURE_2D, gBuffer->materialIDTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gBuffer->materialIDTexture, 0);

	gl.GenTextures(1, &gBuffer->normalTexture);
	gl.BindTexture(GL_TEXTURE_2D, gBuffer->normalTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, screenWidth, screenHeight, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gBuffer->normalTexture, 0);

	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthVelocityBuffer->depthTexture, 0);

	gl.DrawBuffers(2, bufferAttachments);


	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void fb_DestroyGBuffer(rdGBuffer *gBuffer)
{
	gl.DeleteTextures(1, &gBuffer->normalTexture);
	gl.DeleteTextures(1, &gBuffer->materialIDTexture);
	gl.DeleteFramebuffers(1, &gBuffer->framebuf);
}

static void fb_SetupColorBuffer(rdColorBuffer *colorBuffer, int width, int height)
{
	colorBuffer->pixWidth  = width;
	colorBuffer->pixHeight = height;

	gl.GenFramebuffers(1, &colorBuffer->framebuf);
	gl.BindFramebuffer(GL_FRAMEBUFFER, colorBuffer->framebuf);

	gl.GenTextures(1, &colorBuffer->colorTexture);
	gl.BindTexture(GL_TEXTURE_2D, colorBuffer->colorTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer->colorTexture, 0);

	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void fb_DestroyColorBuffer(rdColorBuffer *colorBuffer)
{
	gl.DeleteTextures(1, &colorBuffer->colorTexture);
	gl.DeleteFramebuffers(1, &colorBuffer->framebuf);
}

static void fb_SetupShadowsBuffer(rdShadowsBuffer *shadowsBuffer,
                                  const rdDepthVelocityBuffer *depthVelocityBuffer, int screenWidth,
                                  int screenHeight)
{
	gl.GenFramebuffers(1, &shadowsBuffer->framebuf);
	gl.BindFramebuffer(GL_FRAMEBUFFER, shadowsBuffer->framebuf);

	gl.GenTextures(1, &shadowsBuffer->shadowsTexture);
	gl.BindTexture(GL_TEXTURE_2D, shadowsBuffer->shadowsTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, screenWidth, screenHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                        shadowsBuffer->shadowsTexture, 0);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
	                        depthVelocityBuffer->depthTexture, 0);

	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

static void fb_DestroyShadowsBuffer(rdShadowsBuffer *shadowsBuffer)
{
	gl.DeleteTextures(1, &shadowsBuffer->shadowsTexture);
	gl.DeleteFramebuffers(1, &shadowsBuffer->framebuf);
}

static void fb_SetupBloomBuffer(rdBloomBuffer *bloomBuffer,
	                            const rdDepthVelocityBuffer *depthVelocityBuffer, int width,
	                            int height, int blurWidth, int blurHeight)
{
	bloomBuffer->width  = width;
	bloomBuffer->height = height;

	bloomBuffer->blurWidth  = blurWidth;
	bloomBuffer->blurHeight = blurHeight;

	gl.GenFramebuffers(1, &bloomBuffer->framebufRaw);
	gl.BindFramebuffer(GL_FRAMEBUFFER, bloomBuffer->framebufRaw);

	gl.GenTextures(1, &bloomBuffer->bloomRawTexture);
	gl.BindTexture(GL_TEXTURE_2D, bloomBuffer->bloomRawTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                        bloomBuffer->bloomRawTexture, 0);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
	                        depthVelocityBuffer->depthTexture, 0);
	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	gl.GenFramebuffers(1, &bloomBuffer->framebufBlur1);
	gl.BindFramebuffer(GL_FRAMEBUFFER, bloomBuffer->framebufBlur1);

	gl.GenTextures(1, &bloomBuffer->bloomBlur1Texture);
	gl.BindTexture(GL_TEXTURE_2D, bloomBuffer->bloomBlur1Texture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, blurWidth, blurHeight, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                        bloomBuffer->bloomBlur1Texture, 0);
	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

	gl.GenFramebuffers(1, &bloomBuffer->framebufBlur2);
	gl.BindFramebuffer(GL_FRAMEBUFFER, bloomBuffer->framebufBlur2);

	gl.GenTextures(1, &bloomBuffer->bloomBlur2Texture);
	gl.BindTexture(GL_TEXTURE_2D, bloomBuffer->bloomBlur2Texture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, blurWidth, blurHeight, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                        bloomBuffer->bloomBlur2Texture, 0);
	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
}

static void fb_DestroyBloomBuffer(rdBloomBuffer *bloomBuffer)
{
	gl.DeleteTextures(1, &bloomBuffer->bloomRawTexture);
	gl.DeleteFramebuffers(1, &bloomBuffer->framebufRaw);

	gl.DeleteTextures(1, &bloomBuffer->bloomBlur1Texture);
	gl.DeleteFramebuffers(1, &bloomBuffer->framebufBlur1);	

	gl.DeleteTextures(1, &bloomBuffer->bloomBlur2Texture);
	gl.DeleteFramebuffers(1, &bloomBuffer->framebufBlur2);	
}

static void fb_SetupAmbientOcclusionBuffer(rdSSAOBuffer *ssaoBuffer, int width, int height)
{
	rdVec3 noise[16];

	ssaoBuffer->width  = width;
	ssaoBuffer->height = height;

	for (int i = 0; i < 64; i++) {
		rdVec3  sample;
		float   scale;

		sample = vc_Vec3(ma_Random(-1.0f, 1.0f),
		                 ma_Random(-1.0f, 1.0f),
		                 ma_Random( 0.0f, 1.0f));
		vc_Normalize(&sample);

		scale = (float) i / 64.0f;
		scale = ma_Lerp(0.1f, 1.0f, scale * scale);

		sample = vc_MultiScalar(&sample, scale);
		ssaoBuffer->kernel[i] = sample;
	}

	for (int i = 0; i < 16; i++) {
		noise[i] = vc_Vec3(ma_Random(-1.0f, 1.0f), ma_Random(-1.0f, 1.0f), 0.0f);
	}
	gl.GenTextures(1, &ssaoBuffer->noiseTexture);
	gl.BindTexture(GL_TEXTURE_2D, ssaoBuffer->noiseTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &noise[0]);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	gl.GenFramebuffers(1, &ssaoBuffer->framebufRaw);
	gl.BindFramebuffer(GL_FRAMEBUFFER, ssaoBuffer->framebufRaw);

	gl.GenTextures(1, &ssaoBuffer->occlusionRawTexture);
	gl.BindTexture(GL_TEXTURE_2D, ssaoBuffer->occlusionRawTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                        ssaoBuffer->occlusionRawTexture, 0);

	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);

	gl.GenFramebuffers(1, &ssaoBuffer->framebufBlur);
	gl.BindFramebuffer(GL_FRAMEBUFFER, ssaoBuffer->framebufBlur);

	gl.GenTextures(1, &ssaoBuffer->occlusionBlurTexture);
	gl.BindTexture(GL_TEXTURE_2D, ssaoBuffer->occlusionBlurTexture);
	gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RGB, GL_FLOAT, NULL);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                        ssaoBuffer->occlusionBlurTexture, 0);

	assert(gl.CheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
	gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void fb_DestroyAmbientOcclusionBuffer(rdSSAOBuffer *ssaoBuffer)
{
	gl.DeleteTextures(1, &ssaoBuffer->occlusionBlurTexture);
	gl.DeleteFramebuffers(1, &ssaoBuffer->framebufBlur);

	gl.DeleteTextures(1, &ssaoBuffer->occlusionRawTexture);
	gl.DeleteFramebuffers(1, &ssaoBuffer->framebufRaw);

	gl.DeleteTextures(1, &ssaoBuffer->noiseTexture);
}

static void sh_SetupShader(rdShader *shader, const char *sourceVertex,
                           const char *sourceFragment)
{
	GLchar debugBuf[2048];
	GLint  status;

	shader->vertexShader = gl.CreateShader(GL_VERTEX_SHADER);
	gl.ShaderSource(shader->vertexShader, 1, &sourceVertex, NULL);
	gl.CompileShader(shader->vertexShader);
	gl.GetShaderiv(shader->vertexShader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gl.GetShaderInfoLog(shader->vertexShader, sizeof (debugBuf) - 1, NULL, debugBuf);
		printf("Error compiling vertex shader: %s", debugBuf);
		assert(status == GL_TRUE);
	}

	shader->fragmentShader = gl.CreateShader(GL_FRAGMENT_SHADER);
	gl.ShaderSource(shader->fragmentShader, 1, &sourceFragment, NULL);
	gl.CompileShader(shader->fragmentShader);
	gl.GetShaderiv(shader->fragmentShader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		gl.GetShaderInfoLog(shader->fragmentShader, sizeof (debugBuf) - 1, NULL, debugBuf);
		printf("Error compiling fragment shader: %s", debugBuf);
		assert(status == GL_TRUE);
	}

	shader->shaderProgram = gl.CreateProgram();
	gl.AttachShader(shader->shaderProgram, shader->vertexShader);
	gl.AttachShader(shader->shaderProgram, shader->fragmentShader);
	gl.LinkProgram(shader->shaderProgram);
	gl.GetProgramiv(shader->shaderProgram, GL_LINK_STATUS, &status);
	if (status == GL_FALSE) {
		gl.GetProgramInfoLog(shader->shaderProgram, sizeof (debugBuf) - 1, NULL, debugBuf);
		printf("Error linking shader program: %s", debugBuf);
		assert(status == GL_TRUE);
	}
}

static void sh_DestroyShader(rdShader *shader)
{
	gl.DeleteShader(shader->vertexShader);
	gl.DeleteShader(shader->fragmentShader);
	gl.DeleteProgram(shader->shaderProgram);
}

static void sh_SetupUniform(rdShader *shader, int index, const char *name)
{
	assert(index >= 0 && index < 16);

	shader->uniforms[index] = gl.GetUniformLocation(shader->shaderProgram, name);
}

static void me_GenerateNormalsIndexed(rdVec3 *outNormals, int numVertices, const rdVertex *vertices,
                                      int numIndices, const rdIndex *indices)
{
	const int  numTriangles = numIndices / 3;

	rdTriangle triangles[numTriangles];
	rdVec3     triNormals[numTriangles];

	for (int i = 0; i < numVertices; i++)
		vc_Zero(&outNormals[i]);

	for (int i = 0; i < numTriangles; i++) {
		const rdVec3 *v1, *v2, *v3;

		v1 = (const rdVec3 *) &vertices[indices[i * 3 + 0]];
		v2 = (const rdVec3 *) &vertices[indices[i * 3 + 1]];
		v3 = (const rdVec3 *) &vertices[indices[i * 3 + 2]];

		triangles[i]  = tr_FromVertices(v1, v2, v3);
		triNormals[i] = tr_Normal(&triangles[i]);
	}

	for (int i = 0; i < numVertices; i++) {
		for (int j = 0; j < numTriangles; j++) {
			const rdVec3 *vec;
			
			vec = (const rdVec3 *) &vertices[i];
			if (tr_IsAdjacent(&triangles[j], vec))
				outNormals[i] = vc_Add(&outNormals[i], &triNormals[j]);
		}
		vc_Normalize(&outNormals[i]);
	}
}

static void me_GenerateNormalsNonIndexed(rdVec3 *outNormals, int numVertices,
                                         const rdVertex *vertices)
{
	const int numTriangles = numVertices / 3;

	for (int i = 0; i < numTriangles; i++) {
		const rdVec3 *v1, *v2, *v3;
		rdTriangle    tri;
		rdVec3        normal;

		v1 = (const rdVec3 *) &vertices[i * 3 + 0];
		v2 = (const rdVec3 *) &vertices[i * 3 + 1];
		v3 = (const rdVec3 *) &vertices[i * 3 + 2];

		tri = tr_FromVertices(v1, v2, v3);
		normal = tr_Normal(&tri);
		outNormals[i * 3 + 0] = normal;
		outNormals[i * 3 + 1] = normal;
		outNormals[i * 3 + 2] = normal;
	}
}

static void me_InvertNormals(rdVec3 *normals, int numNormals)
{
	for (int i = 0; i < numNormals; i++)
		vc_Invert(&normals[i]);
}

static rdTriangle tr_FromVertices(const rdVec3 *v1, const rdVec3 *v2, const rdVec3 *v3)
{
	rdTriangle tri;

	tri.v1 = *v1;
	tri.v2 = *v2;
	tri.v3 = *v3;

	return tri;
}

static rdVec3 tr_Normal(const rdTriangle *tri)
{
	rdVec3 normal;
	rdVec3 edge1, edge2;

	edge1 = vc_Sub(&tri->v2, &tri->v1);
	edge2 = vc_Sub(&tri->v3, &tri->v1);

	normal = vc_Cross(&edge1, &edge2);
	vc_Normalize(&normal);

	return normal;
}

static int tr_IsAdjacent(const rdTriangle *tri, const rdVec3 *vec)
{
	if (vc_Equal(&tri->v1, vec))
		return 1;
	if (vc_Equal(&tri->v2, vec))
		return 1;
	if (vc_Equal(&tri->v3, vec))
		return 1;
	return 0;
}

static void mx_Identity(rdMat4 *mat)
{
	mat->m[0][0] = 1.0f; mat->m[0][1] = 0.0f; mat->m[0][2] = 0.0f; mat->m[0][3] = 0.0f;
	mat->m[1][0] = 0.0f; mat->m[1][1] = 1.0f; mat->m[1][2] = 0.0f; mat->m[1][3] = 0.0f;
	mat->m[2][0] = 0.0f; mat->m[2][1] = 0.0f; mat->m[2][2] = 1.0f; mat->m[2][3] = 0.0f;
	mat->m[3][0] = 0.0f; mat->m[3][1] = 0.0f; mat->m[3][2] = 0.0f; mat->m[3][3] = 1.0f;
}

static void mx_Zero(rdMat4 *mat)
{
	mat->m[0][0] = 0.0f; mat->m[0][1] = 0.0f; mat->m[0][2] = 0.0f; mat->m[0][3] = 0.0f;
	mat->m[1][0] = 0.0f; mat->m[1][1] = 0.0f; mat->m[1][2] = 0.0f; mat->m[1][3] = 0.0f;
	mat->m[2][0] = 0.0f; mat->m[2][1] = 0.0f; mat->m[2][2] = 0.0f; mat->m[2][3] = 0.0f;
	mat->m[3][0] = 0.0f; mat->m[3][1] = 0.0f; mat->m[3][2] = 0.0f; mat->m[3][3] = 0.0f;
}

static void mx_Translate(rdMat4 *mat, float x, float y, float z)
{
	rdMat4 translate;

	mx_Identity(&translate);
	translate.m[0][3] = x;
	translate.m[1][3] = y;
	translate.m[2][3] = z;
	mx_MultiAB(mat, &translate, mat);
}

static void mx_Scale(rdMat4 *mat, float x, float y, float z)
{
	rdMat4 scale;

	mx_Identity(&scale);
	scale.m[0][0] = x;
	scale.m[1][1] = y;
	scale.m[2][2] = z;

	mx_MultiAB(mat, &scale, mat);
}

static void mx_Rotate(rdMat4 *mat, float x, float y, float z)
{
	rdMat4 rotX, rotY, rotZ;
	rdMat4 original, yToX, xToZ;

	mx_Identity(&rotX);
	mx_Identity(&rotY);
	mx_Identity(&rotZ);

	original = *mat;

	rotX.m[1][1] =  cosf(x);
	rotX.m[1][2] = -sinf(x);
	rotX.m[2][1] =  sinf(x);
	rotX.m[2][2] =  cosf(x);

	rotY.m[0][0] =  cosf(y);
	rotY.m[0][2] =  sinf(y);
	rotY.m[2][0] = -sinf(y);
	rotY.m[2][2] =  cosf(y);

	rotZ.m[0][0] =  cosf(z);
	rotZ.m[0][1] = -sinf(z);
	rotZ.m[1][0] =  sinf(z);
	rotZ.m[1][1] =  cosf(z);

	mx_MultiAB(&yToX, &rotY, &rotX);
	mx_MultiAB(&xToZ, &yToX, &rotZ);
	mx_MultiAB(mat, &original, &xToZ);
}

static void mx_LookAt(rdMat4 *mat, const rdVec3 *position, const rdVec3 *target, const rdVec3 *up)
{
	rdVec3 f, s, u;

	mx_Identity(mat);

	f = vc_Sub(target, position);
	vc_Normalize(&f);
	s = vc_Cross(&f, up);
	vc_Normalize(&s);
	u = vc_Cross(&s, &f);

	mat->m[0][0] =  s.x;
	mat->m[0][1] =  s.y;
	mat->m[0][2] =  s.z;
	mat->m[1][0] =  u.x;
	mat->m[1][1] =  u.y;
	mat->m[1][2] =  u.z;
	mat->m[2][0] = -f.x;
	mat->m[2][1] = -f.y;
	mat->m[2][2] = -f.z;
	mat->m[0][3] = -vc_Dot(&s, position);
	mat->m[1][3] = -vc_Dot(&u, position);
	mat->m[2][3] =  vc_Dot(&f, position);
}

static void mx_Frustum(rdMat4 *p, double left, double right, double bottom, double top,
                       double zNear, double zFar)
{
	mx_Zero(p);

	p->m[0][0] =  (2 * zNear) / (right - left);
	p->m[0][2] =  (right + left) / (right - left);
	p->m[1][1] =  (2 * zNear) / (top - bottom);
	p->m[1][2] =  (top + bottom) / (top - bottom);
	p->m[2][2] = -((zFar + zNear) / (zFar - zNear));
	p->m[2][3] = -((2 * zFar * zNear) / (zFar - zNear));
	p->m[3][2] = -1.0f;
}

static void mx_InvertFrustum(rdMat4 *p)
{
	rdMat4 tmp;

	double a = p->m[0][0];
	double b = p->m[1][1];
	double c = p->m[2][2];
	double d = p->m[2][3];
	double e = p->m[3][2];

	mx_Zero(&tmp);

	tmp.m[0][0] = 1.0f / a;
	tmp.m[1][1] = 1.0f / b;
	tmp.m[3][2] = 1.0f / d;
	tmp.m[2][3] = 1.0f / e;
	tmp.m[3][3] = -c / (d * e);

	*p = tmp;
}
#if 0
static void mx_Ortho(rdMat4 *o, double left, double right, double bottom, double top, double near,
                     double far)
{
	mx_Identity(o);

	o->m[0][0] = 2 / (right - left);
	o->m[0][3] = - (right + left) / (right - left);
	o->m[1][1] = 2 / (top - bottom);
	o->m[1][3] = - (top + bottom) / (top - bottom);
	o->m[2][2] = -2 / (far - near);
	o->m[2][3] = - (far + near) / (far - near);
}
#endif
static void mx_MultiAB(rdMat4 *out, const rdMat4 *a, const rdMat4 *b)
{
	out->m[0][0] = a->m[0][0] * b->m[0][0] + a->m[0][1] * b->m[1][0] + a->m[0][2] * b->m[2][0] + a->m[0][3] * b->m[3][0];
	out->m[0][1] = a->m[0][0] * b->m[0][1] + a->m[0][1] * b->m[1][1] + a->m[0][2] * b->m[2][1] + a->m[0][3] * b->m[3][1];
	out->m[0][2] = a->m[0][0] * b->m[0][2] + a->m[0][1] * b->m[1][2] + a->m[0][2] * b->m[2][2] + a->m[0][3] * b->m[3][2];
	out->m[0][3] = a->m[0][0] * b->m[0][3] + a->m[0][1] * b->m[1][3] + a->m[0][2] * b->m[2][3] + a->m[0][3] * b->m[3][3];

	out->m[1][0] = a->m[1][0] * b->m[0][0] + a->m[1][1] * b->m[1][0] + a->m[1][2] * b->m[2][0] + a->m[1][3] * b->m[3][0];
	out->m[1][1] = a->m[1][0] * b->m[0][1] + a->m[1][1] * b->m[1][1] + a->m[1][2] * b->m[2][1] + a->m[1][3] * b->m[3][1];
	out->m[1][2] = a->m[1][0] * b->m[0][2] + a->m[1][1] * b->m[1][2] + a->m[1][2] * b->m[2][2] + a->m[1][3] * b->m[3][2];
	out->m[1][3] = a->m[1][0] * b->m[0][3] + a->m[1][1] * b->m[1][3] + a->m[1][2] * b->m[2][3] + a->m[1][3] * b->m[3][3];

	out->m[2][0] = a->m[2][0] * b->m[0][0] + a->m[2][1] * b->m[1][0] + a->m[2][2] * b->m[2][0] + a->m[2][3] * b->m[3][0];
	out->m[2][1] = a->m[2][0] * b->m[0][1] + a->m[2][1] * b->m[1][1] + a->m[2][2] * b->m[2][1] + a->m[2][3] * b->m[3][1];
	out->m[2][2] = a->m[2][0] * b->m[0][2] + a->m[2][1] * b->m[1][2] + a->m[2][2] * b->m[2][2] + a->m[2][3] * b->m[3][2];
	out->m[2][3] = a->m[2][0] * b->m[0][3] + a->m[2][1] * b->m[1][3] + a->m[2][2] * b->m[2][3] + a->m[2][3] * b->m[3][3];

	out->m[3][0] = a->m[3][0] * b->m[0][0] + a->m[3][1] * b->m[1][0] + a->m[3][2] * b->m[2][0] + a->m[3][3] * b->m[3][0];
	out->m[3][1] = a->m[3][0] * b->m[0][1] + a->m[3][1] * b->m[1][1] + a->m[3][2] * b->m[2][1] + a->m[3][3] * b->m[3][1];
	out->m[3][2] = a->m[3][0] * b->m[0][2] + a->m[3][1] * b->m[1][2] + a->m[3][2] * b->m[2][2] + a->m[3][3] * b->m[3][2];
	out->m[3][3] = a->m[3][0] * b->m[0][3] + a->m[3][1] * b->m[1][3] + a->m[3][2] * b->m[2][3] + a->m[3][3] * b->m[3][3];
}

static void mx_MultiABC(rdMat4 *out, const rdMat4 *a, const rdMat4 *b, const rdMat4 *c)
{
	rdMat4 tmp;

	mx_MultiAB(&tmp, a, b);
	mx_MultiAB(out, &tmp, c);
}

static rdVec3 mx_MultiVector3(const rdMat3 *mat, const rdVec3 *vec)
{
	rdVec3 ret;

	ret.x = mat->m[0][0] * vec->x + mat->m[0][1] * vec->y + mat->m[0][2] * vec->z;
	ret.y = mat->m[1][0] * vec->x + mat->m[1][1] * vec->y + mat->m[1][2] * vec->z;
	ret.z = mat->m[2][0] * vec->x + mat->m[2][1] * vec->y + mat->m[2][2] * vec->z;

	return ret;
}

static rdVec4 mx_MultiVector4(const rdMat4 *mat, const rdVec4 *vec)
{
	rdVec4 ret;

	ret.x = mat->m[0][0] * vec->x + mat->m[0][1] * vec->y + mat->m[0][2] * vec->z + mat->m[0][3] * vec->w;
	ret.y = mat->m[1][0] * vec->x + mat->m[1][1] * vec->y + mat->m[1][2] * vec->z + mat->m[1][3] * vec->w;
	ret.z = mat->m[2][0] * vec->x + mat->m[2][1] * vec->y + mat->m[2][2] * vec->z + mat->m[2][3] * vec->w;
	ret.w = mat->m[3][0] * vec->x + mat->m[3][1] * vec->y + mat->m[3][2] * vec->z + mat->m[3][3] * vec->w;

	return ret;
}

static rdMat3 mx_Mat3From4(const rdMat4 *src)
{
	rdMat3 ret;

	ret.m[0][0] = src->m[0][0];
	ret.m[0][1] = src->m[0][1];
	ret.m[0][2] = src->m[0][2];
	ret.m[1][0] = src->m[1][0];
	ret.m[1][1] = src->m[1][1];
	ret.m[1][2] = src->m[1][2];
	ret.m[2][0] = src->m[2][0];
	ret.m[2][1] = src->m[2][1];
	ret.m[2][2] = src->m[2][2];

	return ret;
}

static void mx_Invert3(rdMat3 *mat)
{
	float tmp[3][3];
	float det, invDet;

    tmp[0][0] = mat->m[1][1] * mat->m[2][2] - mat->m[1][2] * mat->m[2][1];
    tmp[0][1] = mat->m[0][2] * mat->m[2][1] - mat->m[0][1] * mat->m[2][2];
    tmp[0][2] = mat->m[0][1] * mat->m[1][2] - mat->m[0][2] * mat->m[1][1];
    tmp[1][0] = mat->m[1][2] * mat->m[2][0] - mat->m[1][0] * mat->m[2][2];
    tmp[1][1] = mat->m[0][0] * mat->m[2][2] - mat->m[0][2] * mat->m[2][0];
    tmp[1][2] = mat->m[0][2] * mat->m[1][0] - mat->m[0][0] * mat->m[1][2];
    tmp[2][0] = mat->m[1][0] * mat->m[2][1] - mat->m[1][1] * mat->m[2][0];
    tmp[2][1] = mat->m[0][1] * mat->m[2][0] - mat->m[0][0] * mat->m[2][1];
    tmp[2][2] = mat->m[0][0] * mat->m[1][1] - mat->m[0][1] * mat->m[1][0];

	det = mat->m[0][0] * tmp[0][0] + mat->m[0][1] * tmp[1][0] + mat->m[0][2] * tmp[2][0];

	if (fabs(det) <= 0.00001f) {
		return;
	}

	invDet = 1.0f / det;

    mat->m[0][0] = invDet * tmp[0][0];
    mat->m[0][1] = invDet * tmp[0][1];
    mat->m[0][2] = invDet * tmp[0][2];
    mat->m[1][0] = invDet * tmp[1][0];
    mat->m[1][1] = invDet * tmp[1][1];
    mat->m[1][2] = invDet * tmp[1][2];
    mat->m[2][0] = invDet * tmp[2][0];
    mat->m[2][1] = invDet * tmp[2][1];
    mat->m[2][2] = invDet * tmp[2][2];
}

static void mx_Transpose3(rdMat3 *mat)
{
	rdMat3 tmp;

	tmp.m[0][0] = mat->m[0][0];
	tmp.m[0][1] = mat->m[1][0];
	tmp.m[0][2] = mat->m[2][0];
	tmp.m[1][0] = mat->m[0][1];
	tmp.m[1][1] = mat->m[1][1];
	tmp.m[1][2] = mat->m[2][1];
	tmp.m[2][0] = mat->m[0][2];
	tmp.m[2][1] = mat->m[1][2];
	tmp.m[2][2] = mat->m[2][2];

	*mat = tmp;
}

static rdVec2 vc_Vec2(float x, float y)
{
	rdVec2 vec;

	vec.x = x;
	vec.y = y;

	return vec;
}

static rdVec3 vc_Vec3(float x, float y, float z)
{
	rdVec3 vec;

	vec.x = x;
	vec.y = y;
	vec.z = z;

	return vec;
}

static rdVec4 vc_Vec4(float x, float y, float z, float w)
{
	rdVec4 vec;

	vec.x = x;
	vec.y = y;
	vec.z = z;
	vec.w = w;

	return vec;
}

static void vc_Zero(rdVec3 *vec)
{
	vec->x = 0.0f;
	vec->y = 0.0f;
	vec->z = 0.0f;
}

static rdVec3 vc_Add(const rdVec3 *a, const rdVec3 *b)
{
	rdVec3 vec;

	vec.x = a->x + b->x;
	vec.y = a->y + b->y;
	vec.z = a->z + b->z;

	return vec;
}

static rdVec3 vc_Sub(const rdVec3 *a, const rdVec3 *b)
{
	rdVec3 vec;

	vec.x = a->x - b->x;
	vec.y = a->y - b->y;
	vec.z = a->z - b->z;

	return vec;
}

static rdVec3 vc_Multiply(const rdVec3 *a, const rdVec3 *b)
{
	rdVec3 vec;

	vec.x = a->x * b->x;
	vec.y = a->y * b->y;
	vec.z = a->z * b->z;

	return vec;
}

static rdVec3 vc_MultiScalar(const rdVec3 *vec, float scalar)
{
	rdVec3 ret;

	ret.x = vec->x * scalar;
	ret.y = vec->y * scalar;
	ret.z = vec->z * scalar;

	return ret;
}
#if 0
static rdVec3 vc_DivideScalar(const rdVec3 *vec, float scalar)
{
	rdVec3 ret;

	ret.x = vec->x / scalar;
	ret.y = vec->y / scalar;
	ret.z = vec->z / scalar;

	return ret;
}
#endif
static int vc_Equal(const rdVec3 *a, const rdVec3 *b)
{
	if (a->x == b->x && a->y == b->y && a->z == b->z)
		return 1;
	return 0;
}

static void vc_Normalize(rdVec3 *vec)
{
	float len;

	len = sqrt((vec->x * vec->x) + (vec->y * vec->y) + (vec->z * vec->z));
	if (len == 0.0f)
		return;

	vec->x = vec->x / len;
	vec->y = vec->y / len;
	vec->z = vec->z / len;
}

static void vc_Invert(rdVec3 *vec)
{
	if (vec->x != 0.0f)
		vec->x = -vec->x;
	if (vec->y != 0.0f)
		vec->y = -vec->y;
	if (vec->z != 0.0f)
		vec->z = -vec->z;
}

static rdVec3 vc_Cross(const rdVec3 *a, const rdVec3 *b)
{
	rdVec3 cross;

	cross.x = (a->y * b->z) - (b->y * a->z);
	cross.y = (a->z * b->x) - (b->z * a->x);
	cross.z = (a->x * b->y) - (b->x * a->y);
	
	return cross;
}

static float vc_Dot(const rdVec3 *a, const rdVec3 *b)
{
	rdVec3 tmp;

	tmp = vc_Multiply(a, b);
	return tmp.x + tmp.y + tmp.z;
}

static float ma_ToRadians(float degrees)
{
	return degrees * (RD_PI / 180.0f);
}

static float ma_WrapAngle(float angle)
{
	const float limitAbs = 360.0f;

	while (angle > limitAbs)
		angle -= limitAbs;
	while (angle < -limitAbs)
		angle += limitAbs;
	return angle;
}

static float ma_Clamp(float val, float min, float max)
{
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}

static float ma_Random(float min, float max)
{
	float s = rand() / (float) RAND_MAX;
	
	return min + s * (max - min);
}

static float ma_Lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

static float ma_Halton(unsigned int i, unsigned int base)
{
	float f = 1.0f;
	float r = 0.0;

	int curr = i;

	do
	{
		f /= base;
		r = r + f * (curr % base);
		curr = floorf(curr / base);
	} while (curr > 0);

	return r;
}
