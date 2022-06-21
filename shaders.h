#ifndef SHADERS_H
#define SHADERS_H

#define GLSL(version, shader) "#version " #version "\n" #shader

#endif

static const char *shaderSourceDepthOnlyVertex = GLSL(410 core,
	layout (location = 0) in vec3 vPosition;

	uniform mat4 mMVP;

	void main(void)
	{
		gl_Position = mMVP * vec4(vPosition, 1.0);
	}
);

static const char *shaderSourceDepthOnlyFragment = GLSL(410 core,
	void main(void)
	{
		
	}
);

static const char *shaderSourceDepthVelocityVertex = GLSL(410 core,
	layout (location = 0) in vec3 vPosition;

	uniform mat4 mMVP;
	uniform mat4 mPrevMVP;

	out vec4 currPos;
	out vec4 prevPos;

	void main(void)
	{
		currPos = mMVP * vec4(vPosition, 1.0);
		prevPos = mPrevMVP * vec4(vPosition, 1.0);

		gl_Position = currPos;
	}
);

static const char *shaderSourceDepthVelocityFragment = GLSL(410 core,
	layout (location = 0) out vec2 velocity;

	in vec4 currPos;
	in vec4 prevPos;

	uniform vec2 currJitter;
	uniform vec2 prevJitter;

	void main(void)
	{
		vec2 currPosNDC = (currPos.xy / currPos.w) * 0.5 + 0.5;
		vec2 prevPosNDC = (prevPos.xy / prevPos.w) * 0.5 + 0.5;

		velocity = (currPosNDC - currJitter) - (prevPosNDC - prevJitter);
	}
);

static const char *shaderSourceGeometryVertex = GLSL(410 core, 
	layout(location = 0) in vec3 vPosition;
	layout(location = 1) in vec3 vNormal;

	uniform mat4 mMVP;
	uniform mat3 mNormal;
	uniform mat4 mModelView;
	uniform bool paintjob;
	uniform int  materialID;

	out vec3  uNormal;
	out vec3  uFragPos;
	out float uMaterialID;

	int ResolvePaintjob(vec2 n, int materialID);
	float EncodeMaterialID(int id);

	void main(void)
	{
		vec4 viewPos = mModelView * vec4(vPosition, 1.0);

		uFragPos = viewPos.xyz;
		uNormal = mNormal * vNormal;

		int tmpID;

		if (paintjob)
			tmpID = ResolvePaintjob(vNormal.xy, materialID);
		else
			tmpID = materialID;
		uMaterialID = EncodeMaterialID(tmpID);
		gl_Position = mMVP * vec4(vPosition, 1.0);
	}

	int ResolvePaintjob(vec2 n, int materialID)
	{
		int id;

		if (n.y > 0)
			id = materialID + 0;
		else if (n.x > 0)
			id = materialID + 1;
		else if (n.x < 0)
			id = materialID + 2;
		else if (n.y < 0)
			id = materialID + 4;
		else
			id = materialID + 3;

		return id;
	}

	float EncodeMaterialID(int id)
	{
		return float(id) / 255.0;
	}
);

static const char *shaderSourceGeometryFragment = GLSL(410 core,
	layout (location = 0) out float outMaterialID;
	layout (location = 1) out vec2  outNormal;

	in vec3  uNormal;
	in vec3  uFragPos;
	in float uMaterialID;

	vec2 EncodeNormal(vec3 v);

	void main(void)
	{
		outMaterialID = uMaterialID;
		outNormal = EncodeNormal(uNormal);
	}

	vec2 EncodeNormal(vec3 v)
	{
		float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
		vec2  result = v.xy * (1.0 / l1norm);

		if (v.z < 0.0) {
			vec2 snz = vec2(result.x >= 0.0 ? 1.0 : -1.0, result.y >= 0.0 ? 1.0 : -1.0);

			result = (1.0 - abs(result.yx)) * snz;
		}

		return result;
	}
);

static const char *shaderSourceLightingVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPos;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPos.x, vPos.y, 0.0, 1.0);
	}
);

static const char *shaderSourceLightingFragment = GLSL(410 core,
	in  vec2 uUV;
	out vec4 outColor;

	uniform sampler2D depthTexture;

	uniform sampler2D materialIDTexture;
	uniform sampler2D normalTexture;
	
	uniform sampler2D occlusionTexture;
	uniform sampler2D shadowsTexture;

	uniform sampler2D bloomRawTexture;
	uniform sampler2D bloomBlurredTexture;

	uniform mat4 mInvProjection;
	uniform vec3 viewspaceUp;

	uniform vec3 materialColors[64];
	uniform vec3 materialProperties[64];

	uniform int  numLights;
	uniform vec3 lightPositions[64];
	uniform vec3 lightColors[64];
	uniform vec3 lightProperties[64];

	struct Light
	{
		vec3  position;
		vec3  color;

		float intensity;
		float cutoffRadius;
		float upward;
	};

	struct Material
	{
		vec3  color;

		float metalness;
		float roughness;
		float ambient;
	};

	vec3  Illuminate(Light light, Material material, vec3 f0, vec3 v, vec3 n, vec3 l, vec3 h,
		             float distance);
	float Fade(float distance, float cutoffRadius);

	float DistributionTrowbridgeReitz(vec3 n, vec3 h, float roughness);
	float FastGeometrySmith(float dotNV, float dotNL, float roughness);
	float GeometrySchlick(float dot, float roughness);
	vec3  FresnelSchlick(float cosTheta, vec3 f0);

	vec3 PositionFromDepth(float depth, vec2 uv);
	vec3 DecodeNormal(vec2 f);
	int  DecodeMaterialID(float id);

	const float pi = 3.14159265359;

	void main(void)
	{
		vec3  fragPos    = PositionFromDepth(texture(depthTexture, uUV).r, uUV);

		int   materialID = DecodeMaterialID(texture(materialIDTexture, uUV).r);
		vec3  n          = DecodeNormal(texture(normalTexture, uUV).rg);

		float shadow           = texture(shadowsTexture, uUV).r;
		float ambientOcclusion = texture(occlusionTexture, uUV).r;

		float bloom = texture(bloomRawTexture, uUV).r + texture(bloomBlurredTexture, uUV).r;

		Material material;

		material.color     = materialColors[materialID];
		material.metalness = materialProperties[materialID].x;
		material.roughness = materialProperties[materialID].y;
		material.ambient   = materialProperties[materialID].z;

		vec3 v = -fragPos;

		vec3 f0 = mix(vec3(0.04), material.color, material.metalness);

		vec3 lo = vec3(0.0);

		for (int i = 0; i < numLights; i++) {
			Light light;

			light.position = lightPositions[i];
			light.color    = lightColors[i];

			light.intensity    = lightProperties[i].x;
			light.cutoffRadius = lightProperties[i].y;
			light.upward       = lightProperties[i].z;

			float distance = length(light.position - fragPos);

			if (distance > light.cutoffRadius)
				continue;

			vec3 l = mix(normalize(light.position - fragPos), -viewspaceUp, light.upward);
			vec3 h = normalize(v + l);

			vec3  illum = Illuminate(light, material, f0, v, n, l, h, distance);
			float fade  = Fade(distance, light.cutoffRadius);

			lo += illum * fade;
		}

		vec3 tmpColor = material.color * material.ambient + lo;

		tmpColor *= ambientOcclusion;
		tmpColor *= 1.0 - 0.85 * shadow;

		tmpColor = tmpColor / (tmpColor + 1.0);

		tmpColor += vec3(bloom);

		outColor = vec4(tmpColor, 1.0);
	}

	vec3 Illuminate(Light light, Material material, vec3 f0, vec3 v, vec3 n, vec3 l, vec3 h,
	                float distance)
	{
		float attenuation = 1.0 / (distance * distance);
		vec3  radiance    = light.intensity * light.color * attenuation;

		float dotNV = max(dot(n, v), 0.0);
		float dotNL = max(dot(n, l), 0.0);

		float ndf = DistributionTrowbridgeReitz(n, h, material.roughness);
		float g   = FastGeometrySmith(dotNV, dotNL, material.roughness);
		vec3  f   = FresnelSchlick(clamp(dot(h, v), 0.0, 1.0), f0);

		vec3  specNom   = ndf * g * f;
		float specDenom = max(4 * dotNV * dotNL, 0.001);
		vec3  specular  = specNom / specDenom;

		vec3 kd = (1.0 - material.metalness) * (vec3(1.0) - f);

		return (kd * material.color / pi + specular) * radiance * dotNL;
	}

	float Fade(float distance, float cutoffRadius)
	{
		float factor = 1.0;

		if (distance > cutoffRadius - 0.75)
			factor = 1.25 * (cutoffRadius - distance);

		return factor;
	}

	float DistributionTrowbridgeReitz(vec3 n, vec3 h, float roughness)
	{
		float a        = roughness * roughness;
		float aSquared = a * a;

		float dotNH = max(dot(n, h), 0.0);
		float dotNHSquared = dotNH * dotNH;

		float nom = aSquared;
		float denom = (dotNHSquared * (aSquared - 1.0) + 1.0);

		denom = max(pi * denom * denom, 0.00000001);

		return nom / denom;
	}

	float FastGeometrySmith(float dotNV, float dotNL, float roughness)
	{
		float ggx1 = GeometrySchlick(dotNL, roughness);
		float ggx2 = GeometrySchlick(dotNV, roughness);

		return ggx1 * ggx2;
	}

	float GeometrySchlick(float dot, float roughness)
	{
		float r = roughness + 1.0;
		float k = (r * r) / 8.0;

		float nom   = dot;
		float denom = dot * (1.0 - k) + k;

		return nom / denom;
	}

	vec3 FresnelSchlick(float cosTheta, vec3 f0)
	{
		return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
	}

	vec3 PositionFromDepth(float depth, vec2 uv)
	{
		float z = depth * 2.0 - 1.0;

		vec4 posClip = vec4(uv * 2.0 - 1.0, z, 1.0);
		vec4 posView = mInvProjection * posClip;

		posView /= posView.w;

		return posView.xyz;
	}

	vec3 DecodeNormal(vec2 f)
	{
		vec3 v = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
		if (v.z < 0.0) {
			vec2 snz = vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
			v.xy = (1.0 - abs(v.yx)) * snz;
		}
		return normalize(v); 
	}

	int DecodeMaterialID(float id)
	{
		return int(id * 255.0);
	}
);

static const char *shaderSourceSSAOVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceSSAOFragment = GLSL(410 core,
	in  vec2  uUV;
	out float outValue;

	uniform sampler2D depthTexture;
	uniform sampler2D normalTexture;
	uniform sampler2D noiseTexture;

	uniform mat4 mProjection;
	uniform mat4 mInvProjection;

	uniform vec3 samples[64];
	uniform vec2 resolution;

	vec3 PositionFromDepth(float depth, vec2 uv);
	vec3 DecodeNormal(vec2 f);

	const int   kernelSize = 64;
	const float radius     = 0.125;
	const float bias       = 0.050;

	void main(void)
	{
		vec2 noiseScale = vec2(resolution.x / 4.0, resolution.y / 4.0);

		vec3 fragPos = PositionFromDepth(texture(depthTexture, uUV).r, uUV);
		vec3 normal  = DecodeNormal(texture(normalTexture, uUV).xy);

		vec3 randomVec = normalize(texture(noiseTexture, uUV * noiseScale).xyz);

		vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
		vec3 bitangent = cross(normal, tangent);
		mat3 mTBN      = mat3(tangent, bitangent, normal);

		float occlusion = 0.0;

		for (int i = 0; i < kernelSize; i++) {
			vec3 samp = mTBN * samples[i];
			samp = fragPos + samp * radius;

			vec4 offset = vec4(samp, 1.0);
			offset      = mProjection * offset;
			offset.xyz /= offset.w;
			offset.xyz  = offset.xyz * 0.5 + 0.5;

			float sampleDepth = PositionFromDepth(texture(depthTexture, offset.xy).r, uUV).z;

			float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
			occlusion += (sampleDepth >= samp.z + bias ? 1.0 : 0.0) * rangeCheck;
		}
		occlusion = 1.0 - (occlusion / kernelSize);

		outValue = occlusion;
	}

	vec3 PositionFromDepth(float depth, vec2 uv)
	{
		float z = depth * 2.0 - 1.0;

		vec4 posClip = vec4(uv * 2.0 - 1.0, z, 1.0);
		vec4 posView = mInvProjection * posClip;

		posView /= posView.w;

		return posView.xyz;
	}

	vec3 DecodeNormal(vec2 f)
	{
		vec3 v = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
		if (v.z < 0.0) {
			vec2 snz = vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
			v.xy = (1.0 - abs(v.yx)) * snz;
		}
		return normalize(v); 
	}
);

static const char *shaderSourceShadowVertex = GLSL(410 core,
	layout (location = 0) in vec3 vPosition;
	layout (location = 1) in vec3 vNormal;

	uniform mat4 mMVP;
	uniform mat3 mNormal;
	uniform mat4 mModelView;
	uniform mat4 mModelLightspace;

	out vec3 uNormal;
	out vec3 uFragPos;
	out vec4 uFragPosLightspace;

	void main(void)
	{
		uNormal = mNormal * vNormal;
		uFragPos = (mModelView * vec4(vPosition, 1.0)).xyz;
		uFragPosLightspace = mModelLightspace * vec4(vPosition, 1.0);
		gl_Position = mMVP * vec4(vPosition, 1.0);
	}
);

static const char *shaderSourceShadowFragment = GLSL(410 core,
	in  vec3 uNormal;
	in  vec3 uFragPos;
	in  vec4 uFragPosLightspace;
	out float outValue;

	uniform sampler2D shadowMapTexture;

	uniform vec3 actualLightPosition;

	float Shadow(vec4 fragPosLightspace, float bias);

	void main(void)
	{
		vec3  l    = normalize(actualLightPosition - uFragPos);
		float bias = max(0.06 * (1.0 - dot(uNormal, l)), 0.005);

		outValue = Shadow(uFragPosLightspace, bias);
	}

	float Shadow(vec4 fragPosLightspace, float bias)
	{
		float shadow = 0.0;

		vec2 texelSize = 1.0 / textureSize(shadowMapTexture, 0);
		vec3 projUV = fragPosLightspace.xyz / fragPosLightspace.w;
 
		if (projUV.z > 1.0)
			return 0.0;

		projUV = projUV * 0.5 + 0.5;

		float closestDepth = texture(shadowMapTexture, projUV.xy).r;
		float currentDepth = projUV.z;

		for (int x = -2; x <= 2; x++) {
			for (int y = -2; y <= 2; y++) {
				float pcfDepth = texture(shadowMapTexture, projUV.xy + vec2(x, y) * texelSize).r;

				shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
			}
		}
		shadow = shadow / 25.0;
		return shadow;
	}
);

static const char *shaderSourceBloomVertex = GLSL(410 core,
	layout (location = 0) in vec3 vPosition;

	uniform mat4 mMVP;

	void main(void)
	{
		gl_Position = mMVP * vec4(vPosition, 1.0);
	}
);

static const char *shaderSourceBloomFragment = GLSL(410 core,
	out float outValue;

	void main(void)
	{
		outValue = 1.0;
	}
);

static const char *shaderSourceSSRVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceSSRFragment = GLSL(410 core,
	in  vec2 uUV;
	out vec4 outColor;

	uniform mat4 mProjection;
	uniform mat4 mInvProjection;

	uniform sampler2D depthTexture;

	uniform sampler2D materialIDTexture;
	uniform sampler2D normalTexture;

	uniform sampler2D colorTexture;

	uniform float reflectanceProperties[256];

	struct Ray {
		vec3 o;
		vec3 oScreen;
		vec3 viewNormal;
		vec3 d;
		vec3 dScreen;

		vec3 prevPos;
		vec3 currPos;

		int steps;
	};

	vec3  ViewspaceToScreenspace(vec3 vViewspace);
	vec3  PositionFromDepth(float depth, vec2 uv);
	vec3  DecodeNormal(vec2 f);
	int   DecodeMaterialId(float id);
	float min3(vec3 v);
	float max3(vec3 v);

	void main(void)
	{
		const int   maxSteps = 512;
		const float initialStepAmount = 0.003;

		Ray   ray;

		int   materialID  = DecodeMaterialId(texture(materialIDTexture, uUV).r);
		float reflectance = reflectanceProperties[materialID];

		if (reflectance == 0.0) {
			outColor = vec4(0.0);
			return;
		}

		vec3 color = vec3(0.0);

		ray.steps = 0;

		ray.o = PositionFromDepth(texture(depthTexture, uUV).r, uUV);

		vec3 v = normalize(ray.o);

		ray.viewNormal = DecodeNormal(texture(normalTexture, uUV).rg);
		ray.d = normalize(reflect(v, ray.viewNormal));

		vec3 viewFirstStep = ray.o + ray.d;

		ray.oScreen = vec3(uUV, texture(depthTexture, uUV).r);

		vec3 screenFirstStep = ViewspaceToScreenspace(viewFirstStep);
		vec3 stepDist = screenFirstStep - ray.oScreen;

		ray.dScreen = initialStepAmount * normalize(stepDist);

		ray.prevPos = ray.oScreen;
		ray.currPos = ray.prevPos + ray.dScreen;

		while (ray.steps < maxSteps) {
			if (min3(ray.currPos) < 0.0 || max3(ray.currPos) > 1.0) {
				outColor = vec4(0.0);
				return;
			}
			float diff = ray.currPos.z - texture(depthTexture, ray.currPos.xy).r;
			if (diff >= 0.0 && diff < length(ray.dScreen)) {
				color = texture(colorTexture, ray.currPos.xy).rgb;
				break;
			}
			ray.prevPos = ray.currPos;
			ray.currPos = ray.prevPos + ray.dScreen;
			ray.steps++;
		}
		outColor = vec4(color, 1.0);
	}

	vec3 ViewspaceToScreenspace(vec3 vViewspace)
	{
		vec4 tmp = mProjection * vec4(vViewspace, 1.0);

		tmp /= tmp.w;

		tmp = tmp * 0.5 + 0.5;

		return tmp.xyz;
	}

	vec3 PositionFromDepth(float depth, vec2 uv)
	{
		float z = depth * 2.0 - 1.0;

		vec4 posClip = vec4(uv * 2.0 - 1.0, z, 1.0);
		vec4 posView = mInvProjection * posClip;

		posView /= posView.w;

		return posView.xyz;
	}

	vec3 DecodeNormal(vec2 f)
	{
		vec3 v = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
		if (v.z < 0.0) {
			vec2 snz = vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
			v.xy = (1.0 - abs(v.yx)) * snz;
		}
		return normalize(v); 
	}

	int DecodeMaterialId(float id)
	{
		return int(id * 255.0);
	}

	float min3(vec3 v)
	{
		return min(min(v.x, v.y), v.z);
	}

	float max3(vec3 v)
	{
		return max(max(v.x, v.y), v.z);
	}
);

static const char *shaderSourceCompositeVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceCompositeFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D colorTexture;
	uniform sampler2D reflectionsTexture;
	uniform sampler2D materialIDTexture;

	uniform float reflectanceProperties[64];

	int DecodeMaterialId(float id);

	void main(void)
	{
		int   materialID  = DecodeMaterialId(texture(materialIDTexture, uUV).r);
		float reflectance = reflectanceProperties[materialID];

		vec3  colorRaw     = texture(colorTexture, uUV).rgb;
		vec3  colorReflect = texture(reflectionsTexture, uUV).rgb;
		float alpha        = texture(reflectionsTexture, uUV).a;

		vec3 tmp = mix(colorRaw, colorReflect, reflectance * alpha);

		outColor = vec4(tmp, 1.0);
	}

	int DecodeMaterialId(float id)
	{
		return int(id * 255.0);
	}
);

/* Special note on temporal anti-aliasing / motion-blur shader:

   TAA resolve function adapted from article by Alex Tardif, found here:
   https://alextardif.com/TAA.html

   Catmull-Rom sampling function adapted from code by Matt Pettineo, found at:
   https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1

   Cubic filter function and ClipAABB both adapted from project also by Matt Pettineo, found at:
   https://github.com/TheRealMJP/MSAAFilter
*/

static const char *shaderSourceTAAResolveMotionBlurVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceTAAResolveMotionBlurFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D colorTexture;
	uniform sampler2D prevColorTexture;

	uniform sampler2D depthTexture;
	uniform sampler2D velocityTexture;

	uniform vec2 resolution;

	vec3  ResolveTAA(void);
	vec3  MotionBlur(void);

	vec4  SampleTextureCatmullRom(sampler2D tex, vec2 texSize, vec2 uv);
	float CubicFilter(float x, float b, float c);

	vec3  ClipAABB(vec3 aabbMin, vec3 aabbMax, vec3 prevSample);

	float Luminance(vec3 c);

	void main(void)
	{

		vec3 colorTAA = ResolveTAA();
		vec3 colorMB  = MotionBlur();

		vec2 velocity = texture(velocityTexture, uUV).rg;

		float factor = (abs(velocity.x) + abs(velocity.y)) * 100.0;

		factor = clamp(factor, 0.0, 1.0);

		outColor = vec4(mix(colorTAA, colorMB, factor), 1.0);
	}

	vec3 ResolveTAA(void)
	{
		vec3  sourceSampleTotal = vec3(0.0);
		float sourceSampleWeight = 0.0;

		vec3 neighborhoodMin = vec3(10000.0);
		vec3 neighborhoodMax = vec3(-10000.0);

		vec3 m1 = vec3(0.0);
		vec3 m2 = vec3(0.0);

		float closestDepth = 0.0;
		vec2 closestDepthPixelPosition = vec2(0.0);

		for (int x = -1; x <= 1; x++) {
			for (int y = - 1; y <= 1; y++) {
				vec2 pixelPosition = uUV.xy + vec2(1.0 / (resolution.x) * x,
				                                   1.0 / (resolution.y) * y);

				pixelPosition = clamp(pixelPosition, 0.0, 1.0);

				vec3 neighbor = max(vec3(0.0), texture(colorTexture, pixelPosition).rgb);

				float subSampleDistance = length(vec2(x, y));
				float subSampleWeight   = CubicFilter(subSampleDistance, 1.0 / 3.0, 1.0 / 3.0);

				sourceSampleTotal  += neighbor * subSampleWeight;
				sourceSampleWeight += subSampleWeight;

				neighborhoodMin = min(neighborhoodMin, neighbor);
				neighborhoodMax = max(neighborhoodMax, neighbor);

				m1 += neighbor;
				m2 += neighbor * neighbor;

				float currentDepth = texture(depthTexture, pixelPosition).r;
				if (currentDepth < closestDepth) {
					closestDepth = currentDepth;
					closestDepthPixelPosition = pixelPosition;
				}
			}
		}
		vec2 motionVector = texture(velocityTexture, closestDepthPixelPosition).xy;
		vec2 historyTexCoord = uUV.xy - motionVector;
		vec3 sourceSample = sourceSampleTotal / sourceSampleWeight;

		if (any(notEqual(historyTexCoord, clamp(historyTexCoord, 0.0, 1.0)))) {
			return sourceSample;
		}

		vec3 historySample = SampleTextureCatmullRom(prevColorTexture, resolution.xy,
		                                             historyTexCoord).rgb;

		float oneDividedBySampleCount = 1.0 / 9.0;
		float gamma = 1.0;
		vec3 mu = m1 * oneDividedBySampleCount;
		vec3 sigma = sqrt(abs((m2 * oneDividedBySampleCount) - (mu * mu)));
		vec3 minC = mu - gamma * sigma;
		vec3 maxC = mu + gamma * sigma;

		historySample = ClipAABB(minC, maxC, clamp(historySample, neighborhoodMin,
		                         neighborhoodMax));

		float sourceWeight  = 0.05;
		float historyWeight = 1.0 - sourceSampleWeight;

		vec3 compressedSource  = sourceSample * 1.0 / (max(max(sourceSample.r, sourceSample.g),
		                                                       sourceSample.b) + 1.0);
		vec3 compressedHistory = historySample * 1.0 / (max(max(historySample.r, historySample.g),
		                                                        historySample.b) + 1.0);

		float luminanceSource  = Luminance(compressedSource);
		float luminanceHistory = Luminance(compressedHistory);

		sourceWeight  *= 1.0 / (1.0 + luminanceSource);
		historyWeight *= 1.0 / (1.0 + luminanceHistory);

		vec3  nom   = (sourceSample * sourceWeight + historySample * historyWeight);
		float denom = max(sourceWeight + historyWeight, 0.00001);

		return nom / denom;
	}

	vec3 MotionBlur(void)
	{
		vec3 result;

		vec2 texelSize = 1.0 / vec2(textureSize(colorTexture, 0));
		vec2 screenUV = gl_FragCoord.xy * texelSize;

		vec2 velocity = texture(velocityTexture, screenUV).rg;

		velocity = clamp(velocity, vec2(-0.1, -0.1), vec2(0.1, 0.1));

		float speed = length(velocity / texelSize);
		int   numSamples = clamp(int(speed), 1, 64);

		result = texture(colorTexture, screenUV).rgb;
		for (int i = 1; i < numSamples; i++) {
			vec2 offset = velocity * (float(i) / float(numSamples - 1) - 0.5);

			result += texture(colorTexture, clamp(screenUV + offset, 0.0, 0.999)).rgb;
		}
		result /= float(numSamples);

		return result;
	}

	vec4 SampleTextureCatmullRom(sampler2D tex, vec2 texSize, vec2 uv)
	{
		vec2 samplePos = uv * texSize;

		vec2 texPos1 = floor(samplePos - 0.5) + 0.5;

		vec2 f = samplePos - texPos1;

		vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));
		vec2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);
		vec2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));
		vec2 w3 = f * f * (-0.5 + 0.5 * f);

		vec2 w12 = w1 + w2;
		vec2 offset12 = w2 / (w1 + w2);

		vec2 texPos0 = texPos1 - 1.0;
		vec2 texPos3 = texPos1 + 2.0;
		vec2 texPos12 = texPos1 + offset12;

		texPos0 /= texSize;
		texPos3 /= texSize;
		texPos12 /= texSize;

		vec4 result = vec4(0.0);

		result += textureLod(tex, vec2(texPos0.x, texPos0.y), 0.0) * w0.x * w0.y;
		result += textureLod(tex, vec2(texPos12.x, texPos0.y), 0.0) * w12.x * w0.y;
		result += textureLod(tex, vec2(texPos3.x, texPos0.y), 0.0) * w3.x * w0.y;

		result += textureLod(tex, vec2(texPos0.x, texPos12.y), 0.0) * w0.x * w12.y;
		result += textureLod(tex, vec2(texPos12.x, texPos12.y), 0.0) * w12.x * w12.y;
		result += textureLod(tex, vec2(texPos3.x, texPos12.y), 0.0) * w3.x * w12.y;

		result += textureLod(tex, vec2(texPos0.x, texPos3.y), 0.0) * w0.x * w3.y;
		result += textureLod(tex, vec2(texPos12.x, texPos3.y), 0.0) * w12.x * w3.y;
		result += textureLod(tex, vec2(texPos3.x, texPos3.y), 0.0) * w3.x * w3.y;

		return result;
	}

	float CubicFilter(float x, float b, float c)
	{
		float y = 0.0;
		float x2 = x * x;
		float x3 = x * x * x;

		if (x < 1.0)
			y = (12.0 - 9.0 * b - 6.0 * c) * x3 + (-18.0 + 12.0 * b + 6.0 * c) * x2 + (6.0 - 2.0 * b);
		else if (x <= 2.0)
			y = (-b - 6.0 * c) * x3 + (6.0 * b + 30.0 * c) * x2 + (-12.0 * b - 48.0 * c) * x + (8.0 * b + 24.0 * c);

		return y / 6.0;
	}

	vec3 ClipAABB(vec3 aabbMin, vec3 aabbMax, vec3 prevSample)
	{
		vec3 pClip = 0.5 * (aabbMax + aabbMin);
		vec3 eClip = 0.5 * (aabbMax - aabbMin);

		vec3 vClip = prevSample - pClip;
		vec3 vUnit = vClip.xyz / eClip;
		vec3 aUnit = abs(vUnit);

		float maUnit = max(aUnit.x, max(aUnit.y, aUnit.z));

		if (maUnit > 1.0)
			return pClip + vClip / maUnit;
		else
			return prevSample;

		return vec3(0.0);
	}

	float Luminance(vec3 c)
	{
		return dot(c, vec3(0.2127, 0.7152, 0.0722));
	}
);

/* Special note on post-process shader:

   Tone-mapping function based on the Uncharted 2 tone-mapper found at:
   https://www.shadertoy.com/view/lslGzl

   Lens-flare function based on code written and generously provided to the public domain by mu6k,
   found at: https://www.shadertoy.com/view/4sX3Rs

*/

static const char *shaderSourcePostProcessVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourcePostProcessFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D colorTexture;

	uniform float randomInput;
	uniform vec2  resolution;

	uniform bool lensFlareEnabled;
	uniform vec2 lensFlarePos;

	vec3  ToneMap(vec3 color);
	vec3  LensFlare(vec2 uv, vec2 pos);
	float random(vec2 p);

	void main(void)
	{
		vec3 color;

		color = texture(colorTexture, uUV).rgb;

		color = ToneMap(color.rgb);

		vec2 uvRandom = uUV;
		uvRandom.y *= random(vec2(uvRandom.y, randomInput));

		color -= 0.03 * random(uvRandom);

		if (lensFlareEnabled) {
			vec2 uvLens = gl_FragCoord.xy / resolution.xy - 0.5;
			uvLens.x *= resolution.x / resolution.y;

			vec2 posLens = lensFlarePos;
			posLens *= resolution.x / resolution.y - 0.5;

			float intensity = 0.85 * clamp(1.0 - (length(posLens) * 2.1) - 0.20, 0.0, 1.0);

			color += intensity * vec3(1.2, 1.1, 1.0) * LensFlare(uvLens, posLens);
		}
		outColor = vec4(color, 1.0);
	}

	vec3 ToneMap(vec3 color)
	{
		const float gamma = 2.2;

		float a = 0.50;
		float b = 0.70;
		float c = 0.12;
		float d = 0.65;
		float e = 0.03;
		float f = 0.35;
		float w = 12.0;

		float exposure = 3.0;

		color *= exposure;
		color = ((color * (a * color + c * b) + d * e) / (color * (a * color + b) + d * f)) - e / f;

		float white = ((w * (a * w + c * b) + d * e) / (w * (a * w + b) + d * f)) - e / f;

		color /= white;
		color = pow(color, vec3(1.0 / gamma));
		return color;
	}

	vec3 LensFlare(vec2 uv, vec2 pos)
	{
		vec2 main = uv - pos;
		vec2 uvd  = uv * (length(uv));

		float ang = atan(main.x, main.y);
		float dist = length(main);
		dist = pow(dist, 0.1);

		float f0 = 1.0 / (length(uv - pos) * 16.0 + 1.0);

		float f1 = max(0.01 - pow(length(uv + 1.2 * pos), 1.9), 0.0) * 7.0;

		float f2  = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.8 * pos), 2.0)), 0.0) * 0.25;
		float f22 = max (1.0 / (1.0 + 32.0 * pow(length(uvd + 0.85 * pos), 2.0)), 0.0) * 0.23;
		float f23 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.9 * pos), 2.0)), 0.0) * 0.21;

		vec2 uvx = mix(uv, uvd, -0.5);

		float f4  = max(0.01 - pow(length(uvx + 0.4 * pos), 2.4), 0.0) * 6.0;
		float f42 = max(0.01 - pow(length(uvx + 0.45 * pos), 2.4), 0.0) * 5.0;
		float f43 = max(0.01 - pow(length(uvx + 0.5 * pos), 2.4), 0.0) * 3.0;

		uvx = mix(uv, uvd, -0.4);

		float f5  = max(0.01 - pow(length(uvx + 0.2 * pos), 5.5), 0.0) * 2.0;
		float f52 = max(0.01 - pow(length(uvx + 0.4 * pos), 5.5), 0.0) * 2.0;
		float f53 = max(0.01 - pow(length(uvx + 0.6 * pos), 5.5), 0.0) * 2.0;

		uvx = mix(uv, uvd, -0.5);

		float f6  = max(0.01 - pow(length(uvx - 0.3 * pos), 1.6), 0.0) * 6.0;
		float f62 = max(0.01 - pow(length(uvx - 0.325 * pos), 1.6), 0.0) * 3.0;
		float f63 = max(0.01 - pow(length(uvx - 0.35 * pos), 1.6), 0.0) * 5.0;

		vec3 c = vec3(0.0);

		c.r +=f2 + f4 + f5 + f6;
		c.g += f22 + f42+ f52 + f62;
		c.b += f23 + f43 + f53 + f63;
		c = c * 1.3 - vec3(length(uvd) * 0.05);

		return c; 

	}

	float random(vec2 p)
	{
		const float gelfond          = 23.14069263277926;
		const float gelfondSchneider = 2.665144142690225;

		vec2 k1 = vec2(gelfond, gelfondSchneider);

		return fract(cos(dot(p, k1)) * 12345.6789);
	}
);

static const char *shaderSourceBlurSingleChannelVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceBlurSingleChannelFragment = GLSL(410 core,
	in  vec2  uUV;
	out float outValue;

	uniform sampler2D inputTexture;

	void main(void)
	{
		vec2 texelSize = 1.0 / vec2(textureSize(inputTexture, 0));
		float result = 0.0;

		for (int x = -2; x < 2; x++) {
			for (int y = -2; y < 2; y++) {
				vec2 offset = vec2(float(x), float(y)) * texelSize;
				result += texture(inputTexture, uUV + offset).r;
			}
		}
		outValue = result / (4.0 * 4.0);
	}
);

static const char *shaderSourceGaussianBlurSingleChannelVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceGaussianBlurSingleChannelFragment = GLSL(410 core,
	in  vec2  uUV;
	out float outValue;

	uniform sampler2D inputTexture;

	uniform bool horizontal;

	void main(void)
	{
		const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

		vec2 texOffset = 1.0 / textureSize(inputTexture, 0);

		float result = texture(inputTexture, uUV).r * weight[0];

		if (horizontal) {
			for (int i = 1; i < 5; i++) {
				result += texture(inputTexture, uUV + vec2(texOffset.x * i, 0.0)).r * weight[i];
				result += texture(inputTexture, uUV - vec2(texOffset.x * i, 0.0)).r * weight[i];
			}
		} else {
			for (int i = 1; i < 5; i++) {
				result += texture(inputTexture, uUV + vec2(texOffset.y * i, 0.0)).r * weight[i];
				result += texture(inputTexture, uUV - vec2(texOffset.y * i, 0.0)).r * weight[i];
			}
		}
		outValue = result;
	}
);

static const char *shaderSourceDebugSingleChannelVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceDebugSingleChannelFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D inputTexture;

	void main(void)
	{
		float c = texture(inputTexture, uUV).r;

		outColor = vec4(c, c, c, 0.0);
	}
);

static const char *shaderSourceDebugDualChannelVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceDebugDualChannelFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D inputTexture;

	void main(void)
	{
		vec2 c = texture(inputTexture, uUV).rg;

		outColor = vec4(c, 0.0, 0.0);
	}
);

static const char *shaderSourceDebugTripleChannelVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceDebugTripleChannelFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D inputTexture;

	void main(void)
	{
		vec3 c = texture(inputTexture, uUV).rgb;

		outColor = vec4(c, 0.0);
	}
);

static const char *shaderSourceDebugNormalsVertex = GLSL(410 core,
	layout (location = 0) in vec2 vPosition;
	layout (location = 1) in vec2 vUV;

	out vec2 uUV;

	void main(void)
	{
		uUV = vUV;
		gl_Position = vec4(vPosition.x, vPosition.y, 0.0, 1.0);
	}
);

static const char *shaderSourceDebugNormalsFragment = GLSL(410 core,
	in  vec2  uUV;
	out vec4  outColor;

	uniform sampler2D normalTexture;

	vec3 DecodeNormal(vec2 f);

	void main(void)
	{
		vec2 e = texture(normalTexture, uUV).rg;

		vec3 d = DecodeNormal(e);

		outColor = vec4(d, 0.0);
	}

	vec3 DecodeNormal(vec2 f)
	{
		vec3 v = vec3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
		if (v.z < 0.0) {
			vec2 snz = vec2(v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0);
			v.xy = (1.0 - abs(v.yx)) * snz;
		}
		return normalize(v); 
	}
);
