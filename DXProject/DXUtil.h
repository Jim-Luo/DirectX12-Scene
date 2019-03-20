#pragma once

#include <Windows.h>
#include <WindowsX.h>
#include <wrl.h>
#include <comdef.h>
using namespace Microsoft::WRL;


#pragma comment (lib,"d3dcompiler.lib")
#pragma comment (lib,"D3D12.lib")
#pragma comment (lib,"dxgi.lib")

#include <dxgi1_4.h>
#include <d3d12.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <xstring>
#include <vector>
#include <array>
#include <unordered_map>
#include "d3dx12.h"
#include "GameTimer.h"
#include "DDSTextureLoader.h"

#include <fbxsdk.h>


#define PI 3.1415926535f

using namespace DirectX;


class MathHelper
{
public:
    static XMFLOAT4X4 Identity4x4()
    {
	        static DirectX::XMFLOAT4X4 I(
		    1.0f, 0.0f, 0.0f, 0.0f,
		    0.0f, 1.0f, 0.0f, 0.0f,
		    0.0f, 0.0f, 1.0f, 0.0f,
		    0.0f, 0.0f, 0.0f, 1.0f);

	    return I;
    }

	template <typename T>
	static T Clamp(const T &x, const T &low, const T &high)
	{
		return x < low ? low :( x > high ? high : x);
	}
};

struct ObjectConstants
{
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();
};

struct PassConstants
{
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();

	XMFLOAT3 EyePos = { 0.0f,0.0f,0.0f };
	XMFLOAT2 RenderTargetSize = { 0.0f,0.0f };
	XMFLOAT2 InvRenderTargetSize = { 0.0f,0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
};

struct Material
{
	std::string matName;

	int matCBIndex = -1;
	int numFrameDirty = 3;
	int diffuseSRVHeapIndex = -1;

	XMFLOAT4 diffuseAlbedo = { 1.0f,1.0f,1.0f,1.0f };
	XMFLOAT3 fresnelR0 = { 0.1f,0.1f,0.1f };
	float roughness = 0.25f;

	XMFLOAT4X4 matTransform = MathHelper::Identity4x4();

};
struct MaterialConstants
{
	XMFLOAT4 diffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	XMFLOAT3 fresnelR0 = XMFLOAT3(0.0f, 0.0f, 0.0f);
	float roughness = 0.0f;

	XMFLOAT4X4 materialTransform = MathHelper::Identity4x4();
};
struct Light
{
	XMFLOAT3 strength = { 0.5f,0.5f,0.5f };
	XMFLOAT3 direction = { 0.0f,-1.0f,0.0f };
};
struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};
struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT3 nor;
	XMFLOAT4 col;
	XMFLOAT2 tex;
};

template<typename T>
class UploadBuffer
{
private:
	bool isConstantBuffer = false;
	UINT elementByteSize;
	BYTE* mMappedData = nullptr;
	ComPtr<ID3D12Resource> mUploadBuffer;
public:
	UploadBuffer(ID3D12Device* mDevice, UINT elementCount, bool isConstantBuffer)
		:isConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);
		if (isConstantBuffer)
			elementByteSize = DXUtil::calConstantBufferByteSize(sizeof(T));
		mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer)
		);
		mUploadBuffer->SetName(L"Upload Buffer");

		mUploadBuffer->Map(0, nullptr, reinterpret_cast<void **>(&mMappedData));
	}
	~UploadBuffer()
	{
		if (mUploadBuffer != nullptr)
			mUploadBuffer->Unmap(0, nullptr);
		mMappedData = nullptr;
	}
	ID3D12Resource* getUploadBuffer()
	{
		return mUploadBuffer.Get();
	}
	void CopyData(UINT index,const T& data)
	{
		memcpy(&mMappedData[index * elementByteSize], &data, sizeof(T));
	}

};
class FrameResource
{
public:
	FrameResource(ID3D12Device *mDevice, UINT passCount, UINT objCount);
	FrameResource(ID3D12Device *mDevice, UINT passCount, UINT objCount, UINT matCount);
	~FrameResource();

	ComPtr<ID3D12CommandAllocator> mCmdAlloc;
	std::unique_ptr<UploadBuffer<ObjectConstants>> objCB = nullptr;
	std::unique_ptr<UploadBuffer<PassConstants>> passCB = nullptr;
	std::unique_ptr<UploadBuffer<MaterialConstants>> matCB = nullptr;


	UINT64 Fence = 0;
};

struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;

	BoundingBox box;
};

struct MeshGeometry
{
	std::string Name;

	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	ComPtr<ID3D12Resource> VertexBufferUpload = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUpload = nullptr;

	UINT VertexBufferByteSize = 0;
	UINT VertexBufferStride = 0;
	UINT IndexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	D3D12_VERTEX_BUFFER_VIEW getVertexBufferView()
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.SizeInBytes = VertexBufferByteSize;
		vbv.StrideInBytes = VertexBufferStride;
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW getIndexBufferView()
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;
		return ibv;
	}

	void disposeUpload()
	{
		VertexBufferUpload = nullptr;
		IndexBufferUpload = nullptr;
	}
};
class GeometryGenerator
{
public:
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;

	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 tex;

		Vertex() {};
		Vertex(const XMFLOAT3 &p):
			pos(p) {};
		Vertex(const XMFLOAT3 &p, const XMFLOAT2 &u) :
			pos(p), tex(u) {};
		Vertex(float x, float y, float z) :
			pos(x, y, z) {};
		Vertex(float x, float y, float z, float u, float v) :
			pos(x, y, z), tex(u, v) {};
	};
	struct Vertex_t
	{
		XMFLOAT3 pos;
		XMFLOAT3 nor;
		XMFLOAT4 col;
		XMFLOAT2 tex;

		Vertex_t() {};
		Vertex_t(XMFLOAT3 pos, XMFLOAT3 nor, XMFLOAT4 col, XMFLOAT2 tex) :
			pos(pos), nor(nor), col(col), tex(tex) {};
		Vertex_t(float x, float y, float z, float m, float n, float o, float r, float g, float b, float a, float u, float v) :
			pos(x, y, z), nor(m, n, o), col(r, g, b, a), tex(u, v) {};
	};

	struct MeshData
	{
		std::vector<Vertex> Vertices;
		std::vector<uint32> Indices32;
		std::vector<uint16>& getIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (UINT i = 0; i < Indices32.size(); ++i)
				{
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}
			}
			return mIndices16;
		}
	private:
		std::vector<uint16> mIndices16;
	};

	struct MeshData_t
	{
		std::vector<Vertex_t> Vertices;
		std::vector<uint32> Indices32;
		std::vector<uint16>& getIndices16()
		{
			if (mIndices16.empty())
			{
				mIndices16.resize(Indices32.size());
				for (UINT i = 0; i < Indices32.size(); ++i)
				{
					mIndices16[i] = static_cast<uint16>(Indices32[i]);
				}
			}
			return mIndices16;
		}
	private:
		std::vector<uint16> mIndices16;
	};
	MeshData CreateGrid(float width, float depth, uint32 m, uint32 n);
};
struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	UINT FrameResourceDirty = 3;
	UINT objCBIndex = -1;

	D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	MeshGeometry* Geo = nullptr;
	Material* Mat = nullptr;

	UINT IndexCount = 0;
	UINT BaseVertexLocation = 0;
	UINT StartIndexLocation = 0;

};

enum class RenderLayer :int
{
	Opaque = 0,
	Sky,
	Camp,
	Count
};
class DXUtil
{
public:
	static  UINT calConstantBufferByteSize(UINT byteSize);
	static ComPtr<ID3D12Resource> createDefaultBuffer(ID3D12Device *mDevice, ID3D12GraphicsCommandList *mCmdList, void *initData, UINT64 byteSize, ComPtr<ID3D12Resource> &uploadBuffer);
	static ComPtr<ID3DBlob> CompileShader(const std::wstring& filename,
		D3D_SHADER_MACRO* defines,
		const std::string& entryPoint,
		const std::string& target);

	DXUtil();
	~DXUtil();
};

