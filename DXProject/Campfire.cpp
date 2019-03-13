#include "stdafx.h"
#include "Campfire.h"

bool Campfire::initDirect3D()
{
	if (!DXSample::initDirect3D())
		return false;

	
	mCmdList->Reset(mCmdAlloc.Get(), nullptr);
	
	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	loadTexture();
	buildRootSignature();
	buildDescriptorHeaps();
	buildShaderAndInputLayout();
	buildGeometry();
	buildMaterials();
	buildRenderItem();
	buildFrameResource();
	buildPSO();
	
	mCmdList->Close();
	ID3D12CommandList* lists[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(lists), lists);
	Flush();

	return true;

}
void Campfire::Resize()
{
	DXSample::Resize();

	mCamera.SetLens(0.25f*PI, RATIO, 1.0f, 1000.0f);
}
void Campfire::Update(const GameTimer& gt)
{
	
	OnKeyboardInput(gt);
	//Update Frame Resource
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % 3;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	//Update Fence
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle);
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	//Update Object Constant Buffer
	auto currObjectCB = mCurrFrameResource->objCB.get();
	for (auto &e : mAllRitems)
	{
		if (e->FrameResourceDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
			ObjectConstants objConstants;
			
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
			currObjectCB->CopyData(e->objCBIndex, objConstants);

			e->FrameResourceDirty--;

		}
	}

	//Update Pass Constants
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePos = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;

	auto currPassCB = mCurrFrameResource->passCB.get();
	currPassCB->CopyData(0, mMainPassCB);

	auto currMaterialCB = mCurrFrameResource->matCB.get();
	for (auto &e : mMaterials)
	{
		Material *mat = e.second.get();
		if (mat->numFrameDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->matTransform);

			MaterialConstants matConstants;
			matConstants.diffuseAlbedo = mat->diffuseAlbedo;
			matConstants.fresnelR0 = mat->fresnelR0;
			matConstants.roughness = mat->roughness;

			XMStoreFloat4x4(&matConstants.materialTransform,XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->matCBIndex, matConstants);

			mat->numFrameDirty--;

		}

	}


	

}
void Campfire::Draw()
{
	auto aCmdAlloc = mCurrFrameResource->mCmdAlloc;
	aCmdAlloc->Reset();
	mCmdList->Reset(aCmdAlloc.Get(), mPSOs["opaque"].Get());
	

	mCmdList->RSSetViewports(1, &mViewport);
	mCmdList->RSSetScissorRects(1, &mRect);
	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(getCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	mCmdList->ClearRenderTargetView(getCurrentBackBufferView(), Colors::AliceBlue, 0, nullptr);
	mCmdList->ClearDepthStencilView(getDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCmdList->OMSetRenderTargets(1, &getCurrentBackBufferView(), true, &getDepthStencilView());

	ID3D12DescriptorHeap *descriptorheap[] = { mSrvDescriptorHeap.Get() };
	mCmdList->SetDescriptorHeaps(_countof(descriptorheap), descriptorheap);

	mCmdList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->passCB->getUploadBuffer();
	mCmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);


	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(getCurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	
	mCmdList->Close();

	
	ID3D12CommandList* cmdsLists[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	
	mSwapChain->Present(0, 0);
	mCurrentBackBuffer = (mCurrentBackBuffer + 1) % 2;

	mCurrFrameResource->Fence = ++mCurrentFence;

	mCmdQueue->Signal(mFence.Get(), mCurrentFence);
}
void Campfire::OnLMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(ghWnd);
}
void Campfire::OnLMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}
void Campfire::OnRMouseDown(WPARAM btnState, int x, int y)
{
	
	mIsMoving = true;
	
	
}
void Campfire::OnRMouseUp(WPARAM btnState, int x, int y)
{
	

	
	mIsMoving = false;
	
}
void Campfire::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.15f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.15f * static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);

	}
	if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.15f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.15f * static_cast<float>(y - mLastMousePos.y));
		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
		mCamera.Walk(0.5f);
	}
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}


void Campfire::OnKeyboardInput(const GameTimer &gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f*dt);

	mCamera.UpdateViewMatrix();
}
Campfire::Campfire()
{
}


Campfire::~Campfire()
{
}


void Campfire::loadTexture()
{
	auto grass = std::make_unique<Texture>();
	grass->Name = "grass";
	grass->Filename = L"grass.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), grass->Filename.c_str(), grass->Resource,grass->UploadHeap);

	mTextures["grass"] = std::move(grass);
}

void Campfire::buildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(mSrvDescriptorHeap.GetAddressOf()));

	auto grass = mTextures["grass"]->Resource;

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = grass->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = -1;
	mDevice->CreateShaderResourceView(grass.Get(), &srvDesc, hDesc);
}

void Campfire::buildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER slotRootPara[4];
	slotRootPara[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);//tex
	slotRootPara[1].InitAsConstantBufferView(0);//obj
	slotRootPara[2].InitAsConstantBufferView(1);//pas
	slotRootPara[3].InitAsConstantBufferView(2);//mat

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init(_countof(slotRootPara), slotRootPara, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> error = nullptr;

	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), error.GetAddressOf());

	if (error != nullptr)
		OutputDebugStringA((char *)error->GetBufferPointer());

	

	mDevice->CreateRootSignature(0, 
		serializedRootSig->GetBufferPointer(), 
		serializedRootSig->GetBufferSize(), 
		IID_PPV_ARGS(mRootSignature.GetAddressOf()));

}

void Campfire::buildShaderAndInputLayout()
{
	mShaders["standardVS"] = DXUtil::CompileShader(L"colors.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = DXUtil::CompileShader(L"colors.hlsl", nullptr, "PS", "ps_5_0");


	mInputLayout = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	    {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	    {"TEXCOORD",0, DXGI_FORMAT_R32G32_FLOAT,0,40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
}

void Campfire::buildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->matCBIndex = 0;
	grass->matName = "grass";
	grass->diffuseSRVHeapIndex = 0;
	grass->diffuseAlbedo = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	grass->fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	grass->roughness = 0.7f;

	mMaterials["grass"] = std::move(grass);
}

void Campfire::buildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(500.0f, 500.0f, 50, 50);


	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].pos;
		vertices[i].pos = p;
		vertices[i].pos.y =0.1*( 0.3f*(p.z*sinf(0.1f*p.x) + p.x * cosf(0.1f*p.z)));
		vertices[i].nor = getHillsNormal(p.x, p.z);
		vertices[i].tex = grid.Vertices[i].tex;
		if (vertices[i].pos.y < -10.0f)
		{
			vertices[i].col = XMFLOAT4(1.0f, 0.96f, 0.62f, 1.0f);
		}
		else if (vertices[i].pos.y < 5.0f)
		{
			vertices[i].col = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
		}
		else if (vertices[i].pos.y < 12.0f)
		{
			vertices[i].col = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.0f);
		}
		else if (vertices[i].pos.y < 20.0f)
		{
			vertices[i].col = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.0f);
		}
		else
		{
			vertices[i].col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = grid.getIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "camp";
	

	D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU);
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU);
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUpload);

	geo->IndexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), indices.data(), ibByteSize, geo->IndexBufferUpload);

	geo->VertexBufferStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexBufferByteSize = ibByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["camp"] = std::move(geo);
	
}


void Campfire::buildRenderItem()
{
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	gridRitem->Geo = mGeometries["camp"].get();
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->objCBIndex = 0;
	gridRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	mAllRitems.push_back(std::move(gridRitem));



}

void Campfire::buildFrameResource()
{
	for (UINT i = 0; i < 3; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(mDevice.Get(), 1, (UINT)mAllRitems.size(),(UINT)mMaterials.size()));
	}
}

void Campfire::buildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePSO;

	ZeroMemory(&opaquePSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePSO.SampleMask = UINT_MAX;
	opaquePSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePSO.NumRenderTargets = 1;
	opaquePSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	opaquePSO.pRootSignature = mRootSignature.Get();
	opaquePSO.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	opaquePSO.SampleDesc.Count = 1;
	opaquePSO.SampleDesc.Quality = 0;
	opaquePSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	opaquePSO.VS =
	{
		reinterpret_cast<BYTE *>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePSO.PS =
	{
		reinterpret_cast<BYTE *>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	mDevice->CreateGraphicsPipelineState(&opaquePSO, IID_PPV_ARGS(mPSOs["opaque"].GetAddressOf()));


}

void Campfire::drawRenderItems(ID3D12GraphicsCommandList * cmdList, const std::vector<RenderItem*>& ritems)
{

	UINT objCBByteSize = DXUtil::calConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = DXUtil::calConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->objCB->getUploadBuffer();
	auto materialCB = mCurrFrameResource->matCB->getUploadBuffer();
	

	
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->getVertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->getIndexBufferView());
		cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->diffuseSRVHeapIndex, mCBVSRVUAVDescriptorSize);
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress();
		objCBAddress += ri->objCBIndex *objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = materialCB->GetGPUVirtualAddress();
		matCBAddress += ri->Mat->matCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);

	}
	
	
	
}

XMFLOAT3 Campfire::getHillsNormal(float x, float z) const
{
	XMFLOAT3 n(
		-0.03f*z*cosf(0.1f*x) - 0.3f*cosf(0.1f*z),
		1.1f,
		-0.3f*sinf(0.1f*x) + 0.03f*x*sinf(0.1f*z)
	);
	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);
	return n;
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> Campfire::GetStaticSamplers()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}




