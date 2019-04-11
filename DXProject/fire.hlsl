
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
	float3 PosW   : POSITION;
	float4 Color  : COLOR;
};
struct GeoOut
{
	float4 PosH   : SV_POSITION;
	float3 PosW   : POSITION;
	float4 Color  : COLOR;
	float2 TexC   : TEXCOORD;
	uint PrimID   : SV_PrimitiveID;
};
VertexOut VS(VertexIn vin)
{
	VertexOut vout;

	// Transform to homogeneous clip space.
	float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
	vout.PosW = posW.xyz;

	// Just pass vertex color into the pixel shader.
	vout.Color = vin.Color;
	return vout;
}

[maxvertexcount(4)]
void GS(point VertexOut gin[1],
	uint primID:SV_PrimitiveID,
	inout TriangleStream<GeoOut> triStream)
{
	
	float4 col = gin[0].Color;

	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 look = gEyePosW - gin[0].PosW;
	look.y = 0.0f; 
	look = normalize(look);
	float3 right = cross(up, look);

	
	float halfWidth =0.5f;
	float halfHeight = 1.00f;

	float4 v[3];
	v[0] = float4(gin[0].PosW + halfWidth * right - halfHeight * up, 1.0f);
	v[1] = float4(gin[0].PosW + halfHeight * up, 1.0f);
	v[2] = float4(gin[0].PosW - halfWidth * right - halfHeight * up, 1.0f);

	float2 texC[3] =
	{
		float2(0.5f,0.0f),
		float2(1.0f,1.0f),
		float2(0.0f,1.0f)
	};


	GeoOut gout;
	[unroll]
	for (int i = 0; i < 3; ++i)
	{
		gout.PosH = mul(v[i], gViewProj);
		gout.PosW = v[i].xyz;
		gout.Color = col;
		gout.TexC = texC[i];
		gout.PrimID = primID;

		triStream.Append(gout);
	}
	
}

float4 PS(GeoOut pin) : SV_Target
{
	float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap,pin.TexC);
	float4 lit = diffuseAlbedo * pin.Color;
	return  lit;
}

