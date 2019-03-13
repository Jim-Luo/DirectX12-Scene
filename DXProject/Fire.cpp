#include "stdafx.h"
#include "Fire.h"



Fire::Fire()
{

}
Fire::~Fire()
{
}

void Fire::initFire(UINT64 ParticleCount, 
	ComPtr<ID3D12Device> m_device, 
	ComPtr<ID3D12GraphicsCommandList> m_commandList, 
	ComPtr<ID3D12DescriptorHeap> m_srvUavHeap, 
	UINT m_rtvDescriptorSize, UINT m_srvUavDescriptorSize)
{
	this->ParticleCount = ParticleCount;
	this->m_device = m_device;
	this->m_commandList = m_commandList;
	this->m_srvUavHeap = m_srvUavHeap;
	this->m_rtvDescriptorSize = m_rtvDescriptorSize;
	this->m_srvUavDescriptorSize = m_srvUavDescriptorSize;
}

void Fire::CreateVertexBuffer()
{
	std::vector<ParticleVertex> vertices;
	vertices.resize(ParticleCount);
	for (UINT i = 0; i < ParticleCount; i++)
	{
		vertices[i].color = XMFLOAT4(1.0f, 1.0f, 0.2f, 1.0f);
	}
	const UINT bufferSize = ParticleCount * sizeof(ParticleVertex);

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer));

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBufferUpload));

	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = reinterpret_cast<UINT8*>(&vertices[0]);
	vertexData.RowPitch = bufferSize;
	vertexData.SlicePitch = vertexData.RowPitch;

	UpdateSubresources<1>(m_commandList.Get(), m_vertexBuffer.Get(), m_vertexBufferUpload.Get(), 0, 0, 1, &vertexData);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

	m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
	m_vertexBufferView.SizeInBytes = static_cast<UINT>(bufferSize);
	m_vertexBufferView.StrideInBytes = sizeof(ParticleVertex);
}

void Fire::CreateParticleBuffers()
{
	// Initialize the data in the buffers.

	std::vector<Particle> data;
	data.resize(ParticleCount);
	const UINT dataSize = ParticleCount * sizeof(Particle);

	// Split the particles into two groups.
	float centerSpread = ParticleSpread ;
	LoadParticles(&data[0], XMFLOAT3(0 , 0, 0), XMFLOAT4(0, 0, 0, 10.0f), ParticleSpread, ParticleCount);
	

	D3D12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(dataSize);
		m_device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_particleBuffer0));

		m_device->CreateCommittedResource(
			&defaultHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_COPY_DEST,
			nullptr,
			IID_PPV_ARGS(&m_particleBuffer1));

		m_device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_particleBuffer0Upload));

		m_device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&uploadBufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_particleBuffer1Upload));

		D3D12_SUBRESOURCE_DATA particleData = {};
		particleData.pData = reinterpret_cast<UINT8*>(&data[0]);
		particleData.RowPitch = dataSize;
		particleData.SlicePitch = particleData.RowPitch;

		UpdateSubresources<1>(m_commandList.Get(), m_particleBuffer0.Get(), m_particleBuffer0Upload.Get(), 0, 0, 1, &particleData);
		UpdateSubresources<1>(m_commandList.Get(), m_particleBuffer1.Get(), m_particleBuffer1Upload.Get(), 0, 0, 1, &particleData);
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuffer0.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));
		m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_particleBuffer1.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = DXGI_FORMAT_UNKNOWN;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
		srvDesc.Buffer.FirstElement = 0;
		srvDesc.Buffer.NumElements = ParticleCount;
		srvDesc.Buffer.StructureByteStride = sizeof(Particle);
		srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle0(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_srvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle1(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_srvUavDescriptorSize);
		m_device->CreateShaderResourceView(m_particleBuffer0.Get(), &srvDesc, srvHandle0);
		m_device->CreateShaderResourceView(m_particleBuffer1.Get(), &srvDesc, srvHandle1);

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = ParticleCount;
		uavDesc.Buffer.StructureByteStride = sizeof(Particle);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle0(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 0, m_srvUavDescriptorSize);
		CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle1(m_srvUavHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_srvUavDescriptorSize);
		m_device->CreateUnorderedAccessView(m_particleBuffer0.Get(), nullptr, &uavDesc, uavHandle0);
		m_device->CreateUnorderedAccessView(m_particleBuffer1.Get(), nullptr, &uavDesc, uavHandle1);
	
}

void Fire::LoadParticles(Particle * pParticles, const XMFLOAT3 & center, const XMFLOAT4 & velocity, float spread, UINT numParticles)
{
	srand(0);
	for (UINT i = 0; i < numParticles; i++)
	{
		XMFLOAT3 delta(spread, spread, spread);

		while (XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&delta))) > spread / 3 * spread / 3)
		{
			delta.x = RandomPercent() * spread;
			delta.y = RandomPercent() * spread;
			delta.z = RandomPercent() * spread;

		}
		pParticles[i].position.x = center.x + delta.x / 5;
		pParticles[i].position.y = center.y + delta.y / 2;
		pParticles[i].position.z = 0/* center.z + delta.z*/;
		pParticles[i].position.w = 10000.0f * 10000.0f;

		pParticles[i].velocity = velocity;

	}
}

float Fire::RandomPercent()
{
	float ret = static_cast<float>((rand() % 10000) - 5000);
	return ret / 5000.0f;
}

void Fire::CreateConstantBuffer()
{
	const UINT bufferSize = sizeof(ConstantBufferCS);

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&m_constantBufferCS));

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(bufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constantBufferCSUpload));

	ConstantBufferCS constantBufferCS = {};
	constantBufferCS.param[0] = ParticleCount;
	constantBufferCS.param[1] = int(ceil(ParticleCount / 128.0f));
	constantBufferCS.paramf[0] = 0.1f;
	constantBufferCS.paramf[1] = 1.0f;

	D3D12_SUBRESOURCE_DATA computeCBData = {};
	computeCBData.pData = reinterpret_cast<UINT8*>(&constantBufferCS);
	computeCBData.RowPitch = bufferSize;
	computeCBData.SlicePitch = computeCBData.RowPitch;

	UpdateSubresources<1>(m_commandList.Get(), m_constantBufferCS.Get(), constantBufferCSUpload.Get(), 0, 0, 1, &computeCBData);
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_constantBufferCS.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));



	const UINT constantBufferGSSize = sizeof(ConstantBufferGS) * 3;

	m_device->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferGSSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_constantBufferGS)
	);

	CD3DX12_RANGE readRange(0, 0);		// We do not intend to read from this resource on the CPU.
	m_constantBufferGS->Map(0, &readRange, reinterpret_cast<void**>(&m_pConstantBufferGSData));
	ZeroMemory(m_pConstantBufferGSData, constantBufferGSSize);
}
