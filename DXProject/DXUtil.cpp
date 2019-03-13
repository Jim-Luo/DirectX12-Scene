#include "stdafx.h"
#include "DXUtil.h"

FrameResource::FrameResource(ID3D12Device *mDevice, UINT passCount, UINT objCount)
{
	mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCmdAlloc.GetAddressOf()));
	objCB = std::make_unique<UploadBuffer<ObjectConstants>>(mDevice, objCount,true);
	passCB = std::make_unique<UploadBuffer<PassConstants>>(mDevice, passCount, true);
}

FrameResource::FrameResource(ID3D12Device * mDevice, UINT passCount, UINT objCount, UINT matCount)
	:FrameResource(mDevice,passCount,objCount)
{
	matCB = std::make_unique<UploadBuffer<MaterialConstants>>(mDevice, matCount, true);
}

FrameResource::~FrameResource()
{

}

UINT DXUtil::calConstantBufferByteSize(UINT byteSize)
{
	return (byteSize + 255) & ~255;
}
ComPtr<ID3D12Resource> DXUtil::createDefaultBuffer(ID3D12Device *mDevice, 
	ID3D12GraphicsCommandList *mCmdList, 
	void *initData, UINT64 byteSize, 
	ComPtr<ID3D12Resource> &uploadBuffer)
{
	ComPtr<ID3D12Resource> defaultBuffer;

	
	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf()));

	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(uploadBuffer.GetAddressOf()));


	
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = subResourceData.RowPitch;

	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources<1>(mCmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

	


	return defaultBuffer;

	
}
ComPtr<ID3DBlob> DXUtil::CompileShader(const std::wstring& filename,
	D3D_SHADER_MACRO* defines,
	const std::string& entryPoint,
	const std::string& target) 
{
	UINT compileFlag = 0;
#if defined(DEBUG) || (_DEBUG)
	compileFlag = D3DCOMPILE_DEBUG || D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	HRESULT hr = S_OK;
	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> error = nullptr;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint.c_str(), target.c_str(), compileFlag, 0, &byteCode, &error);

	if (error != nullptr)
		OutputDebugStringA((char *)error->GetBufferPointer());

	return byteCode;

}

GeometryGenerator::MeshData GeometryGenerator::CreateGrid(float width,float depth,uint32 m,uint32 n)
{
	MeshData meshdata;
	uint32 VertexCount = m * n;
	uint32 FaceCount = (m - 1)*(n - 1) * 2;

	float halfW = 0.5f * width;
	float halfD = 0.5f * depth;

	float dw = width / (n - 1);
	float dd = depth / (m - 1);

	float du = 1.0 / (n-1);
	float dv = 1.0 / (m-1);

	meshdata.Vertices.resize(VertexCount);
	for (uint32 i = 0; i < m; i++)
	{
		float z = halfD - i * dd;
		for (uint32 j = 0; j < n; j++)
		{
			float x = -halfW + j * dw;
			meshdata.Vertices[i*n + j].pos = XMFLOAT3(x, 0.0f, z);

			meshdata.Vertices[i*n + j].tex.x = j * du;
			meshdata.Vertices[i*n + j].tex.y = i * dv;
		}
	}

	meshdata.Indices32.resize(FaceCount * 3);
	uint32 k = 0;
	for (uint32 i = 0; i < m - 1; ++i)
	{
		for (uint32 j = 0; j < n - 1; ++j)
		{
			meshdata.Indices32[k] = i * n + j;
			meshdata.Indices32[k + 1] = i * n + j + 1;
			meshdata.Indices32[k + 2] = (i + 1)*n + j;

			meshdata.Indices32[k + 3] = (i + 1)*n + j;
			meshdata.Indices32[k + 4] = i * n + j + 1;
			meshdata.Indices32[k + 5] = (i + 1)*n + j + 1;

			k += 6; 
		}
	}

	return meshdata;
}

DXUtil::DXUtil()
{
}


DXUtil::~DXUtil()
{
}
