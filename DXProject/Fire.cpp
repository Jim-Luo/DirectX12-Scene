#include "stdafx.h"
#include "Fire.h"



Fire::Fire()
{

}
Fire::Fire(int count, XMFLOAT3 position, XMFLOAT4 color, float dt, int cond)
{

	mVertexCount = count;
	mState = cond;
	mTimeStep = dt;
	mCurrSolution.resize(count);
	mSolutions.resize(cond);
	for (size_t i = 0; i < cond; i++)
	{
		mSolutions[i].resize(count);
		for (size_t j = 0; j < count; j++)
		{
			mSolutions[i][j].color = color;
			mSolutions[i][j].position = XMFLOAT3(position.x + randPos(), position.y + 2 * cos( PI/2 * randPos()), position.z + randPos());
		}
	}
}
void Fire::EnableCS(ID3D12Device * device, ID3D12GraphicsCommandList * cmdList)
{
	mDevice = device;
	buildResource(cmdList);
}
Fire::~Fire()
{

}

void Fire::Update(float dt)
{
	static float t_base = 0.0f;
	t_base += dt;

	UINT solutionIndex = (int)(t_base / mTimeStep) % mState;
	mCurrSolution = mSolutions[solutionIndex];
}

void Fire::UpdateCS( ID3D12GraphicsCommandList * cmdList, ID3D12RootSignature * rootSig, ID3D12PipelineState * pso)
{
	
	

	
	

	cmdList->SetPipelineState(pso);
	cmdList->SetComputeRootSignature(rootSig);

	
		cmdList->SetComputeRoot32BitConstants(0, 3, &mCurrSolution, 0);

		cmdList->SetComputeRootDescriptorTable(1, mSolutionSRV);
		cmdList->SetComputeRootDescriptorTable(2, mSolutionUAV);


		UINT numGroupsX = 1;
		UINT numGroupsY = mVertexCount;
		cmdList->Dispatch(numGroupsX, numGroupsY, 1);

	

		cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSolution.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
	
}

float Fire::randPos()
{
	float ret = static_cast<float>((rand() % 10000) - 5000);
	return ret / 5000.0f;
}

void Fire::buildResource(ID3D12GraphicsCommandList* mCmdList)
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1;
	texDesc.Height = mVertexCount;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R32_FLOAT;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mSolution));

	const UINT num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
	const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mSolution.Get(), 0, num2DSubresources);

	mDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(mUploadBuffer.GetAddressOf()));

	std::vector<float> initData(mVertexCount, 0.0f);
	for (int i = 0; i < initData.size(); ++i)
		initData[i] = 0.0f;

	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData.data();
	subResourceData.RowPitch = sizeof(float);
	subResourceData.SlicePitch = subResourceData.RowPitch * mVertexCount;

	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSolution.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
	UpdateSubresources(mCmdList, mSolution.Get(), mUploadBuffer.Get(), 0, 0, num2DSubresources, &subResourceData);
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSolution.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
}

void Fire::BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

	uavDesc.Format = DXGI_FORMAT_R32_FLOAT;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	mDevice->CreateShaderResourceView(mSolution.Get(), &srvDesc, hCpuDescriptor);
	mDevice->CreateUnorderedAccessView(mSolution.Get(), nullptr, &uavDesc, hCpuDescriptor.Offset(1, descriptorSize));

	mSolutionSRV = hGpuDescriptor;
	mSolutionUAV = hGpuDescriptor.Offset(1, descriptorSize);
}

int Fire::DescriptorCount()
{
	return 2;
}
