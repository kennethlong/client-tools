// ======================================================================
// functions.inc
// HLSL vertex shader functions
// ======================================================================

#include "../../shared_program/functions.inc"

// ======================================================================

float4 transform3d(float4 vertexPosition_o)
{
	return mul(vertexPosition_o, objectWorldCameraProjectionMatrix);
}

// ----------------------------------------------------------------------

float4 transform2d(float2 vertexPosition_s)
{
	float4 result;
	result.x = (vertexPosition_s.x * viewportData.x) + viewportData.z;
	result.y = (vertexPosition_s.y * viewportData.y) + viewportData.w;
	result.z = 0.5;
	result.w = 1.0;
	return result;
}

// ----------------------------------------------------------------------

float calculateFog(float4 vertexPosition_o)
{
	float4 position_w = mul(vertexPosition_o, objectWorldMatrix);
	float3 viewer_w = cameraPosition - position_w;
	float  viewerDistanceSquared = lengthSquared(viewer_w);
	return 1.0 / exp(viewerDistanceSquared * fog.w);
}

// ----------------------------------------------------------------------

float3 transformDot3LightDirection(float3 vertexNormal_o, float4 tcsDOT3)
{
	// build the transformation matrix
	float3x3 m;
	m[0] = (float3)tcsDOT3;
	m[2] = normalize(vertexNormal_o);
	m[1] = cross(m[2], m[0]) * tcsDOT3.w;

	// transform the light direction into tangent space
	return reverseSignAndBias(mul(lightData.dot3[0].direction, transpose(m)));
}

// ----------------------------------------------------------------------

float3 transformTerrainDot3LightDirection(float3 vertexNormal_o)
{
	float3 j = cross(vertexNormal_o, float3(1.0f, 0.0f, 0.0f));
	float3 i = cross(j, vertexNormal_o);

	float3 result;
	result.x = dot(i, lightData.dot3[0].direction);
	result.y = dot(j, lightData.dot3[0].direction);
	result.z = dot(vertexNormal_o, lightData.dot3[0].direction);

	return reverseSignAndBias(result);
}

// ----------------------------------------------------------------------

float4 calculateDiffuseParallelLight(ParallelLight light, float3 vertexNormal_o)
{
	float3 normal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorldMatrix));
	return max(dot(normal_w, light.direction), 0.0) * light.diffuseColor;
}

// ----------------------------------------------------------------------

DiffuseSpecular calculateDiffuseSpecularParallelLight(ParallelSpecularLight light, float4 vertexPosition_o, float3 vertexNormal_o)
{
	float3 viewer_w = normalize(cameraPosition - mul(vertexPosition_o, objectWorldMatrix));
	float3 vertexNormal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorldMatrix));

	float3 halfAngle = normalize(light.direction + viewer_w);
	float  nDotL = dot(vertexNormal_w, light.direction);
	float  nDotH = dot(vertexNormal_w, halfAngle);
	
	float4 lighting = lit(nDotL, nDotH, material.specularPower.x);

	DiffuseSpecular diffuseSpecular;
	diffuseSpecular.diffuse  = lighting.y * light.diffuseColor;
	diffuseSpecular.specular = lighting.z * light.specularColor;

	return diffuseSpecular;
}

// ----------------------------------------------------------------------

float4 calculateDiffusePointLight(PointLight light, float4 vertexPosition_o, float3 vertexNormal_o)
{
	float3 vertexPosition_w = mul(vertexPosition_o, objectWorldMatrix);
	float3 normal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorldMatrix));

	// Get light direction
	float3 lightDirection = light.position - vertexPosition_w;

	// Get light distance squared.
	float lightDistanceSquared = lengthSquared(lightDirection);

	// Get 1/lightDistance
	float oneOverLightDistance = rsqrt(lightDistanceSquared);

	// Normalize light direction
	lightDirection *= oneOverLightDistance;

	// compute distance attenuation
	float4 attenuationFactors;
	attenuationFactors.x = 1.0;
	attenuationFactors.y = lightDistanceSquared * oneOverLightDistance;
	attenuationFactors.z = lightDistanceSquared;
	attenuationFactors.w = oneOverLightDistance;
	float distanceAttenuation = 1.0 / dot(light.attenuation, attenuationFactors);

	return max(dot(normal_w, lightDirection), 0.0) * distanceAttenuation * light.diffuseColor;
}

// ----------------------------------------------------------------------

DiffuseSpecular calculateDiffuseSpecularPointLight(PointSpecularLight light, float3 vertexPosition_o, float3 vertexNormal_o)
{
	float3 vertexPosition_w = mul(vertexPosition_o, objectWorldMatrix);
	float3 normal_w = normalize(mul(vertexNormal_o, (float3x3)objectWorldMatrix));
	float3 viewer_w = normalize(cameraPosition - vertexPosition_w);

	// Get light direction
	float3 lightDirection = light.position - vertexPosition_w;

	// Get light distance squared.
	float lightDistanceSquared = lengthSquared(lightDirection);

	// Get 1/lightDistance
	float oneOverLightDistance = rsqrt(lightDistanceSquared);

	// Normalize light direction
	lightDirection *= oneOverLightDistance;

	// compute distance attenuation
	float4 attenuationFactors;
	attenuationFactors.x = 1.0;
	attenuationFactors.y = lightDistanceSquared * oneOverLightDistance;
	attenuationFactors.z = lightDistanceSquared;
	attenuationFactors.w = 1.0; // oneOverLightDistance;
	float distanceAttenuation = 1.0 / dot(light.attenuation, attenuationFactors);

	float3 halfAngle = normalize(lightDirection + viewer_w);
	float  nDotL = dot(normal_w, lightDirection);
	float  nDotH = dot(normal_w, halfAngle);
	
	float4 lighting = lit(nDotL, nDotH, material.specularPower.x) * distanceAttenuation;

	DiffuseSpecular diffuseSpecular;
	diffuseSpecular.diffuse  = lighting.y * light.diffuseColor;
	diffuseSpecular.specular = lighting.z * light.specularColor;
	return diffuseSpecular;
}

// ----------------------------------------------------------------------

float4 calculateDiffuseLighting(bool dot3, float4 vertexPosition_o, float3 vertexNormal_o)
{
	float4 result = material.emissiveColor;

	if (dot3)
	{
#if VERTEX_SHADER_VERSION >= 20
		if (light_parallelSpecular_0_enabled)
#endif
			result += calculateDiffuseParallelLight((ParallelLight)lightData.parallelSpecular[0], vertexNormal_o);
	}
	else
	{
#if VERTEX_SHADER_VERSION >= 20
		if (light_parallelSpecular_0_enabled)
#endif
			result += calculateDiffuseParallelLight((ParallelLight)lightData.parallelSpecular[0], vertexNormal_o);
	}

#if VERTEX_SHADER_VERSION >= 20
	if (light_parallel_0_enabled)
#endif
		result += calculateDiffuseParallelLight(lightData.parallel[0], vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_parallel_1_enabled)
#endif
		result += calculateDiffuseParallelLight(lightData.parallel[1], vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_pointSpecular_0_enabled)
#endif
		result += calculateDiffusePointLight((PointLight)lightData.pointSpecular[0], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_0_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[0], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_1_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[1], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_2_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[2], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_3_enabled)
#endif
		result += calculateDiffusePointLight(lightData.point[3], vertexPosition_o, vertexNormal_o);

	return result;
}

// ----------------------------------------------------------------------

DiffuseSpecular calculateDiffuseSpecularLighting(bool dot3, float4 vertexPosition_o, float3 vertexNormal_o)
{
	DiffuseSpecular output;
	output.diffuse = material.emissiveColor;
	output.specular = float4(0.0, 0.0, 0.0, 0.0);
	
	if (dot3)
	{
#if VERTEX_SHADER_VERSION >= 20
		if (light_parallelSpecular_0_enabled)
#endif
		{
			DiffuseSpecular temporary = calculateDiffuseSpecularParallelLight(lightData.parallelSpecular[0], vertexPosition_o, vertexNormal_o);
			output.diffuse += temporary.diffuse;
			output.specular += temporary.specular;
		}
	}
	else
	{
#if VERTEX_SHADER_VERSION >= 20
		if (light_parallelSpecular_0_enabled)
#endif
		{
			DiffuseSpecular temporary = calculateDiffuseSpecularParallelLight(lightData.parallelSpecular[0], vertexPosition_o, vertexNormal_o);
			output.diffuse += temporary.diffuse;
			output.specular += temporary.specular;
		}
	}
	
#if VERTEX_SHADER_VERSION >= 20
	if (light_parallel_0_enabled)
#endif
		output.diffuse += calculateDiffuseParallelLight(lightData.parallel[0], vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_parallel_1_enabled)
#endif
		output.diffuse += calculateDiffuseParallelLight(lightData.parallel[1], vertexNormal_o);


#if VERTEX_SHADER_VERSION >= 20
	if (light_pointSpecular_0_enabled)
#endif
	{
		DiffuseSpecular temporary = calculateDiffuseSpecularPointLight(lightData.pointSpecular[0], vertexPosition_o, vertexNormal_o);
		output.diffuse += temporary.diffuse;
		output.specular += temporary.specular;
	}

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_0_enabled)
#endif
		output.diffuse += calculateDiffusePointLight(lightData.point[0], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_1_enabled)
#endif
		output.diffuse += calculateDiffusePointLight(lightData.point[1], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_2_enabled)
#endif
		output.diffuse += calculateDiffusePointLight(lightData.point[2], vertexPosition_o, vertexNormal_o);

#if VERTEX_SHADER_VERSION >= 20
	if (light_point_3_enabled)
#endif
		output.diffuse += calculateDiffusePointLight(lightData.point[3], vertexPosition_o, vertexNormal_o);

	return output;
}

// ======================================================================
