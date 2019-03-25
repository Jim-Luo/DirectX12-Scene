#include "LightingUtil.hlsl"


Texture2D    gDiffuseMap : register(t0);


SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

cbuffer cbPerObject : register(b0)
{
	float4x4 gWorld;
	float4x4 gTexTransform;
};
cbuffer cbPass : register(b1)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float3 gEyePosW;
	float cbPerObjectPad1;
	float2 gRenderTargetSize;
	float2 gInvRenderTargetSize;
	float gNearZ;
	float gFarZ;

};
cbuffer cbMaterial : register(b2)
{
	float4 gDiffuseAlbedo;
	float3 gFresnelR0;
	float  gRoughness;
	float4x4 gMatTransform;
};
struct VertexIn
{
	float3 PosL    : POSITION;
	float3 NormalL : NORMAL;
	float4 Color   : COLOR;
	float2 TexC    : TEXCOORD;
};

struct VertexOut
{
	float4 PosH   : SV_POSITION;
	float3 PosW   : POSITION;
	float3 NormalW: NORMAL;
	float4 Color  : COLOR;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;
	vout.PosH = mul(posW, gViewProj);
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorld);

	// Just pass vertex color into the pixel shader.
	vout.Color = vin.Color;

	float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
	vout.TexC = mul(texC, gMatTransform).xy;

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC) * gDiffuseAlbedo;
	pin.NormalW = normalize(pin.NormalW);
	float3 toEyeW = normalize(gEyePosW - pin.PosW);
	const float shininess = 1.0f - gRoughness;
	float4 ambient = float4(0.25f, 0.25f, 0.3f, 0.0f);
	ambient = ambient * diffuseAlbedo;
	Material mat = { diffuseAlbedo,gFresnelR0, shininess };
	Light l = { float3(0.6f,0.6f,0.4f),float3(1.0f,1.0f,0.0f) };
	float3 directLight = ComputeDirectionalLight(l, mat, pin.NormalW, toEyeW);
	float4 lit = 5 * ambient;
	lit.a = gDiffuseAlbedo.a;
	return lit;
}