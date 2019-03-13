#pragma once

#include "DXUtil.h"

struct Particle
{
	XMFLOAT4 position;
	XMFLOAT4 velocity;

};
struct ParticleVertex
{
	XMFLOAT4 color;
};
class Fire
{
public:
	struct ConstantBufferCS
	{
		UINT param[4];
		float paramf[4];
	};
	ComPtr<ID3D12Resource> m_constantBufferCS;
	ComPtr<ID3D12Resource> constantBufferCSUpload;

	struct ConstantBufferGS
	{
		XMFLOAT4X4 worldViewProjection;
		XMFLOAT4X4 inverseView;
		float padding[32];
	};
	ComPtr<ID3D12Resource> m_constantBufferGS;
	UINT8* m_pConstantBufferGSData;
public:
	UINT64 ParticleCount = 10000U;
	float ParticleSpread = 10000.0f;

	ComPtr<ID3D12Device> m_device;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12Resource> m_vertexBuffer;
	ComPtr<ID3D12Resource> m_vertexBufferUpload;
	
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
	ComPtr<ID3D12Resource> m_particleBuffer0;
	ComPtr<ID3D12Resource> m_particleBuffer1;
	ComPtr<ID3D12Resource> m_particleBuffer0Upload;
	ComPtr<ID3D12Resource> m_particleBuffer1Upload;

	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
	UINT m_rtvDescriptorSize;
	UINT m_srvUavDescriptorSize;

	
public:
	Fire();
	~Fire();

	void initFire(UINT64 ParticleCount, 
		ComPtr<ID3D12Device> m_device, 
		ComPtr<ID3D12GraphicsCommandList> m_commandList, 
		ComPtr<ID3D12DescriptorHeap> m_srvUavHeap, 
		UINT m_rtvDescriptorSize, UINT m_srvUavDescriptorSize);
	void CreateVertexBuffer();
	void CreateParticleBuffers();
	void LoadParticles(_Out_writes_(numParticles) Particle* pParticles,
		const XMFLOAT3& center,
		const XMFLOAT4& velocity,
		float spread, UINT numParticles);
	float RandomPercent();
	void CreateConstantBuffer();

};

