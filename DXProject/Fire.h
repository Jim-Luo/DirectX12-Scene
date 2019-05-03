#pragma once

#include "DXUtil.h"

struct Particle
{
	XMFLOAT3 position;
	XMFLOAT4 color;

};

class Fire
{
public:
	Fire();
	/*
	count: particle count
	color: particle color
	dt: updating time delta
	cond: solution count
	 */
	Fire(int count,XMFLOAT3 position, XMFLOAT4 color, float dt, int cond);
	void EnableCS(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
	~Fire();

	int VertexCount()const { return mVertexCount; }

	// Returns the solution at the ith grid point.
	const Particle& Position(int i)const { return mCurrSolution[i]; }


	void Update(float dt);
	void UpdateCS( ID3D12GraphicsCommandList* cmdList, ID3D12RootSignature* rootSig, ID3D12PipelineState* pso);


private:
	int mVertexCount = 0;
	int mState = 0;
	float mTimeStep = 0.0f;

	float randPos();

	std::vector<Particle> mCurrSolution;
	std::vector<std::vector<Particle>> mSolutions;
	

	ComPtr<ID3D12Device> mDevice;

	CD3DX12_GPU_DESCRIPTOR_HANDLE mSolutionSRV;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mSolutionUAV;

	ComPtr<ID3D12Resource> mSolution;

	ComPtr<ID3D12Resource> mUploadBuffer = nullptr;

public:
	void buildResource(ID3D12GraphicsCommandList* mCmdList);
	void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor, UINT descriptorSize);
	int DescriptorCount();

};

