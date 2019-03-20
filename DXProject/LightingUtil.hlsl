struct Light
{
	float3 strength;
	float3 direction;
};
struct Material
{
	float4 diffuseAlbedo;
	float3 fresnelR0;
	float shininess;
};

float schlickFresnel(float3 R0, float3 normal, float3 lightvec)
{
	float cosIncidentAngle = saturate(dot(normal, lightvec));
	float f0 = 1.0f - cosIncidentAngle;
	float reflectPercent = R0 + (1.0f - R0)*(f0*f0*f0*f0*f0);

	return reflectPercent;
}
float BlinnPhong(float3 lightstrength, float3 lightvec, float3 toeye, float normal, Material mat)
{
	const float m = mat.shininess * 256.0f;
	float3 halfVec = normalize(toeye + lightvec);

	float roughnessFactor = (m + 8.0f)*pow(max(dot(halfVec, normal), 0.0f), m) / 8.0f;
	float3 fresnelFactor = schlickFresnel(mat.fresnelR0, halfVec, lightvec);

	float3 specAlbedo = fresnelFactor * roughnessFactor;

	// Our spec formula goes outside [0,1] range, but we are 
	// doing LDR rendering.  So scale it down a bit.
	specAlbedo = specAlbedo / (specAlbedo + 1.0f);

	return (mat.diffuseAlbedo.rgb + specAlbedo) * lightstrength;
}
float3 ComputeDirectionalLight(Light L, Material mat, float3 normal, float3 toEye)
{
	// The light vector aims opposite the direction the light rays travel.
	float3 lightVec = -L.direction;

	// Scale light down by Lambert's cosine law.
	float ndotl = max(dot(lightVec, normal), 0.0f);
	float3 lightStrength = L.strength * ndotl;

	return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
}
