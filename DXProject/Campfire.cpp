#include "stdafx.h"
#include "Campfire.h"

bool Campfire::initDirect3D()
{
	if (!DXSample::initDirect3D())
		return false;

	
	mCmdList->Reset(mCmdAlloc.Get(), nullptr);
	
	mCamera.SetPosition(0.0f, 10.0f, -15.0f);
	mFires["fire1"] = std::make_unique<Fire>(70, XMFLOAT3(0.0f, 2.0f, 0.0f), XMFLOAT4(0.8f, 0.4f, 0.0f, 1.0f), 0.08f, 10);
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

	updateDynamicElements(gt);
	

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

	mCmdList->ClearRenderTargetView(getCurrentBackBufferView(), Colors::MediumPurple, 0, nullptr);
	mCmdList->ClearDepthStencilView(getDepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	mCmdList->OMSetRenderTargets(1, &getCurrentBackBufferView(), true, &getDepthStencilView());

	ID3D12DescriptorHeap *descriptorheap[] = { mSrvDescriptorHeap.Get() };
	mCmdList->SetDescriptorHeaps(_countof(descriptorheap), descriptorheap);

	mCmdList->SetGraphicsRootSignature(mTerranRootSignature.Get());

	auto passCB = mCurrFrameResource->passCB->getUploadBuffer();
	mCmdList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCmdList->SetPipelineState(mPSOs["sky"].Get());
	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

	mCmdList->SetPipelineState(mPSOs["camp"].Get());
	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Camp]);

	mCmdList->SetPipelineState(mPSOs["fire1"].Get());
	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Fire]);

	mCmdList->SetPipelineState(mPSOs["haystack"].Get());
	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Haystack1]);

	mCmdList->SetPipelineState(mPSOs["haystack"].Get());
	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Haystack2]);

	mCmdList->SetPipelineState(mPSOs["church"].Get());
	drawRenderItems(mCmdList.Get(), mRitemLayer[(int)RenderLayer::Church]);


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
	//grass tex
	auto grass = std::make_unique<Texture>();
	grass->Name = "grass";
	grass->Filename = L"grass.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), grass->Filename.c_str(), grass->Resource,grass->UploadHeap);

	mTextures["grass"] = std::move(grass);

	//cloud tex
	auto sky = std::make_unique<Texture>();
	sky->Name = "sky";
	sky->Filename = L"sky02.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), sky->Filename.c_str(), sky->Resource, sky->UploadHeap);

	mTextures["sky"] = std::move(sky);

	//campfire tex
	auto camp = std::make_unique<Texture>();
	camp->Name = "camp";
	camp->Filename = L"wood03.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), camp->Filename.c_str(), camp->Resource, camp->UploadHeap);

	mTextures["camp"] = std::move(camp);
	
	auto fire = std::make_unique<Texture>();
	fire->Name = "fire";
	fire->Filename = L"fire.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), fire->Filename.c_str(), fire->Resource, fire->UploadHeap);

	mTextures["fire"] = std::move(fire);

	//haystack tex
	auto haystack = std::make_unique<Texture>();
	haystack->Name = "haystack";
	haystack->Filename = L"haystack.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), haystack->Filename.c_str(), haystack->Resource, haystack->UploadHeap);

	mTextures["haystack"] = std::move(haystack);

	//church tex
	auto church = std::make_unique<Texture>();
	church->Name = "church";
	church->Filename = L"church.dds";

	CreateDDSTextureFromFile12(mDevice.Get(), mCmdList.Get(), church->Filename.c_str(), church->Resource, church->UploadHeap);

	mTextures["church"] = std::move(church);


}

void Campfire::buildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NumDescriptors = 6;//
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(mSrvDescriptorHeap.GetAddressOf()));

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	
	auto grass = mTextures["grass"]->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC terranSrvDesc = {};
	terranSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	terranSrvDesc.Format = grass->GetDesc().Format;
	terranSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	terranSrvDesc.Texture2D.MostDetailedMip = 0;
	terranSrvDesc.Texture2D.MipLevels = -1;
	mDevice->CreateShaderResourceView(grass.Get(), &terranSrvDesc, hDesc);

	hDesc.Offset(1, mCBVSRVUAVDescriptorSize);

	auto sky = mTextures["sky"]->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC skySrvDesc = {};
	skySrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	skySrvDesc.Format = sky->GetDesc().Format;
	skySrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	skySrvDesc.Texture2D.MostDetailedMip = 0;
	skySrvDesc.Texture2D.MipLevels = -1;
	mDevice->CreateShaderResourceView(sky.Get(), &skySrvDesc, hDesc);

	hDesc.Offset(1, mCBVSRVUAVDescriptorSize);

	auto camp = mTextures["camp"]->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC campSrvDesc = {};
	campSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	campSrvDesc.Format = camp->GetDesc().Format;
	campSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	campSrvDesc.Texture2DArray.MostDetailedMip = 0;
	campSrvDesc.Texture2DArray.MipLevels = -1;
	campSrvDesc.Texture2DArray.FirstArraySlice = 0;
	campSrvDesc.Texture2DArray.ArraySize = camp->GetDesc().DepthOrArraySize;
	mDevice->CreateShaderResourceView(camp.Get(), &campSrvDesc, hDesc);

	hDesc.Offset(1, mCBVSRVUAVDescriptorSize);

	auto fire = mTextures["fire"]->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC fireSrvDesc = {};
	fireSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	fireSrvDesc.Format = fire->GetDesc().Format;
	fireSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	fireSrvDesc.Texture2D.MostDetailedMip = 0;
	fireSrvDesc.Texture2D.MipLevels = -1;
	mDevice->CreateShaderResourceView(fire.Get(), &fireSrvDesc, hDesc);

	hDesc.Offset(1, mCBVSRVUAVDescriptorSize);

	auto haystack = mTextures["haystack"]->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC haystackSrvDesc = {};
	haystackSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	haystackSrvDesc.Format = haystack->GetDesc().Format;
	haystackSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	haystackSrvDesc.Texture2D.MostDetailedMip = 0;
	haystackSrvDesc.Texture2D.MipLevels = -1;
	mDevice->CreateShaderResourceView(haystack.Get(), &haystackSrvDesc, hDesc);

	hDesc.Offset(1, mCBVSRVUAVDescriptorSize);

	auto church = mTextures["church"]->Resource;
	D3D12_SHADER_RESOURCE_VIEW_DESC churchSrvDesc = {};
	churchSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	churchSrvDesc.Format = church->GetDesc().Format;
	churchSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	churchSrvDesc.Texture2D.MostDetailedMip = 0;
	churchSrvDesc.Texture2D.MipLevels = -1;
	mDevice->CreateShaderResourceView(church.Get(), &churchSrvDesc, hDesc);

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
		IID_PPV_ARGS(mTerranRootSignature.GetAddressOf()));

	CD3DX12_ROOT_PARAMETER skyRootPara[4];
	skyRootPara[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	skyRootPara[1].InitAsConstantBufferView(0);//obj
	skyRootPara[2].InitAsConstantBufferView(1);//pass
	skyRootPara[3].InitAsConstantBufferView(2);//mat

	CD3DX12_ROOT_SIGNATURE_DESC skyRootSigDesc;
	skyRootSigDesc.Init(_countof(skyRootPara), skyRootPara, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> skySerializedRootSig = nullptr;
	ComPtr<ID3DBlob> skyError = nullptr;

	HRESULT hr_sky = D3D12SerializeRootSignature(&skyRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, skySerializedRootSig.GetAddressOf(), skyError.GetAddressOf());

	if (skyError != nullptr)
		OutputDebugStringA((char *)skyError->GetBufferPointer());

	mDevice->CreateRootSignature(0,
		skySerializedRootSig->GetBufferPointer(),
		skySerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mModelRootSignature["sky"].GetAddressOf()));


	CD3DX12_ROOT_PARAMETER campRootPara[4];
	campRootPara[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	campRootPara[1].InitAsConstantBufferView(0);//obj
	campRootPara[2].InitAsConstantBufferView(1);//pass
	campRootPara[3].InitAsConstantBufferView(2);//mat

	CD3DX12_ROOT_SIGNATURE_DESC campRootSigDesc;
	campRootSigDesc.Init(_countof(campRootPara), campRootPara, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> campSerializedRootSig = nullptr;
	ComPtr<ID3DBlob> campError = nullptr;

	HRESULT hr_camp = D3D12SerializeRootSignature(&campRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, campSerializedRootSig.GetAddressOf(), campError.GetAddressOf());

	if (campError != nullptr)
		OutputDebugStringA((char *)campError->GetBufferPointer());

	mDevice->CreateRootSignature(0,
		campSerializedRootSig->GetBufferPointer(),
		campSerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mModelRootSignature["camp"].GetAddressOf()));

	CD3DX12_ROOT_PARAMETER fireRootPara[4];
	fireRootPara[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	fireRootPara[1].InitAsConstantBufferView(0);
	fireRootPara[2].InitAsConstantBufferView(1);
	fireRootPara[3].InitAsConstantBufferView(2);
	CD3DX12_ROOT_SIGNATURE_DESC fireRootSigDesc;
	fireRootSigDesc.Init(_countof(fireRootPara), fireRootPara, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> fireSerializedRootSig = nullptr;
	ComPtr<ID3DBlob> fireError = nullptr;

	HRESULT hr_fire = D3D12SerializeRootSignature(&fireRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, fireSerializedRootSig.GetAddressOf(), fireError.GetAddressOf());

	if (fireError != nullptr)
		OutputDebugStringA((char *)fireError->GetBufferPointer());

	mDevice->CreateRootSignature(0,
		fireSerializedRootSig->GetBufferPointer(),
		fireSerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mEffectRootSignature["fire1"].GetAddressOf()));

	CD3DX12_ROOT_PARAMETER haystackRootPara[4];
	haystackRootPara[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	haystackRootPara[1].InitAsConstantBufferView(0);
	haystackRootPara[2].InitAsConstantBufferView(1);
	haystackRootPara[3].InitAsConstantBufferView(2);
	CD3DX12_ROOT_SIGNATURE_DESC haystackRootSigDesc;
	haystackRootSigDesc.Init(_countof(haystackRootPara), haystackRootPara, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> haystackSerializedRootSig = nullptr;
	ComPtr<ID3DBlob> haystackError = nullptr;

	HRESULT hr_haystack = D3D12SerializeRootSignature(&haystackRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, haystackSerializedRootSig.GetAddressOf(), haystackError.GetAddressOf());

	if (fireError != nullptr)
		OutputDebugStringA((char *)haystackError->GetBufferPointer());

	mDevice->CreateRootSignature(0,
		haystackSerializedRootSig->GetBufferPointer(),
		haystackSerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mModelRootSignature["haystack"].GetAddressOf()));

	CD3DX12_ROOT_PARAMETER churchRootPara[4];
	churchRootPara[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	churchRootPara[1].InitAsConstantBufferView(0);
	churchRootPara[2].InitAsConstantBufferView(1);
	churchRootPara[3].InitAsConstantBufferView(2);
	CD3DX12_ROOT_SIGNATURE_DESC churchRootSigDesc;
	churchRootSigDesc.Init(_countof(churchRootPara), churchRootPara, (UINT)staticSamplers.size(), staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> churchSerializedRootSig = nullptr;
	ComPtr<ID3DBlob> churchError = nullptr;

	HRESULT hr_church = D3D12SerializeRootSignature(&churchRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, churchSerializedRootSig.GetAddressOf(), churchError.GetAddressOf());

	if (churchError != nullptr)
		OutputDebugStringA((char *)churchError->GetBufferPointer());

	mDevice->CreateRootSignature(0,
		churchSerializedRootSig->GetBufferPointer(),
		churchSerializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mModelRootSignature["church"].GetAddressOf()));
}

void Campfire::buildShaderAndInputLayout()
{
	mShaders["standardVS"] = DXUtil::CompileShader(L"colors.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = DXUtil::CompileShader(L"colors.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["skyVS"] = DXUtil::CompileShader(L"sky.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["skyPS"] = DXUtil::CompileShader(L"sky.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["campVS"] = DXUtil::CompileShader(L"camp.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["campPS"] = DXUtil::CompileShader(L"camp.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["fireVS"] = DXUtil::CompileShader(L"fire.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["fireGS"] = DXUtil::CompileShader(L"fire.hlsl", nullptr, "GS", "gs_5_0");
	mShaders["firePS"] = DXUtil::CompileShader(L"fire.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["haystackVS"] = DXUtil::CompileShader(L"haystack.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["haystackPS"] = DXUtil::CompileShader(L"haystack.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["churchVS"] = DXUtil::CompileShader(L"church.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["churchPS"] = DXUtil::CompileShader(L"church.hlsl", nullptr, "PS", "ps_5_0");




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

	auto sky = std::make_unique<Material>();
	sky->matCBIndex = 1;
	sky->matName = "sky";
	sky->diffuseSRVHeapIndex = 1;//!IMPORTANT
	sky->diffuseAlbedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	sky->fresnelR0 = XMFLOAT3(0.8f, 0.8f, 0.8f);
	sky->roughness = 1.0f;

	mMaterials["sky"] = std::move(sky);

	auto camp = std::make_unique<Material>();
	camp->matCBIndex = 2;
	camp->matName = "camp";
	camp->diffuseSRVHeapIndex = 2;//!IMPORTANT
	camp->diffuseAlbedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	camp->fresnelR0 = XMFLOAT3(0.8f, 0.8f, 0.8f);
	camp->roughness = 1.0f;

	mMaterials["camp"] = std::move(camp);

	auto fire = std::make_unique<Material>();
	fire->matCBIndex = 3;
	fire->matName = "fire";
	fire->diffuseSRVHeapIndex = 3;
	fire->diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	fire->fresnelR0 = XMFLOAT3(1.0f, 1.0f, 1.0f);
	fire->roughness = 1.0f;

	mMaterials["fire"] = std::move(fire);

	auto haystack = std::make_unique<Material>();
	haystack->matCBIndex = 4;
	haystack->matName = "haystack";
	haystack->diffuseSRVHeapIndex = 4;
	haystack->diffuseAlbedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	haystack->fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	haystack->roughness = 0.7f;

	mMaterials["haystack"] = std::move(haystack);

	auto church = std::make_unique<Material>();
	church->matCBIndex = 5;
	church->matName = "church";
	church->diffuseSRVHeapIndex = 5;
	church->diffuseAlbedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	church->fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	church->roughness = 0.7f;

	mMaterials["church"] = std::move(church);



}

void Campfire::buildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(700.0f, 700.0f, 50, 50);


	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].pos;
		vertices[i].pos = p;
		vertices[i].pos.y = 0.0f;// 0.1*(0.3f*(p.z*sinf(0.1f*p.x) + p.x * cosf(0.1f*p.z)));
		vertices[i].nor = XMFLOAT3(0.0f, 1.0f, 0.0f);// getHillsNormal(p.x, p.z);
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



	GeometryGenerator::MeshData_t sky;
	mModelLoader.loadModel("mSky.fbx", sky);

	std::vector<Vertex> vertices_sky(sky.Vertices.size());
	for (int i = 0; i < sky.Vertices.size(); ++i)
	{
		vertices_sky[i].pos = sky.Vertices[i].pos;
		vertices_sky[i].col = sky.Vertices[i].col;
		vertices_sky[i].nor = sky.Vertices[i].nor;
		vertices_sky[i].tex.x = (sky.Vertices[i].tex.x + 512) / 1024;
		vertices_sky[i].tex.y = (sky.Vertices[i].tex.y + 512) / 1024;
		
	}
	const UINT vbByteSize_sky = (UINT)vertices_sky.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices_sky = sky.getIndices16();
	const UINT ibByteSize_sky = (UINT)indices_sky.size() * sizeof(std::uint16_t);

	auto geo_sky = std::make_unique<MeshGeometry>();
	geo_sky->Name = "sky";

	D3DCreateBlob(vbByteSize_sky, &geo_sky->VertexBufferCPU);
	CopyMemory(geo_sky->VertexBufferCPU->GetBufferPointer(), vertices_sky.data(), vbByteSize_sky);

	D3DCreateBlob(ibByteSize_sky, &geo_sky->IndexBufferCPU);
	CopyMemory(geo_sky->IndexBufferCPU->GetBufferPointer(), indices_sky.data(), ibByteSize_sky);

	geo_sky->VertexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), vertices_sky.data(), vbByteSize_sky, geo_sky->VertexBufferUpload);

	geo_sky->IndexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), indices_sky.data(), ibByteSize_sky, geo_sky->IndexBufferUpload);

	geo_sky->VertexBufferStride = sizeof(Vertex);
	geo_sky->VertexBufferByteSize = vbByteSize_sky;
	geo_sky->IndexBufferByteSize = ibByteSize_sky;
	geo_sky->IndexFormat = DXGI_FORMAT_R16_UINT;


	SubmeshGeometry submesh_sky;
	submesh_sky.IndexCount = (UINT)indices_sky.size();
	submesh_sky.StartIndexLocation = 0;
	submesh_sky.BaseVertexLocation = 0;

	geo_sky->DrawArgs["sky"] = submesh_sky;

	mGeometries["sky"] = std::move(geo_sky);




	GeometryGenerator::MeshData_t wood;
	mModelLoader.loadModel("campfire1.FBX", wood);

	std::vector<Vertex> vertices_wood(wood.Vertices.size());
	for (int i = 0; i < wood.Vertices.size(); ++i)
	{
		vertices_wood[i].pos = wood.Vertices[i].pos;
		vertices_wood[i].col = wood.Vertices[i].col;
		vertices_wood[i].nor = wood.Vertices[i].nor;
		vertices_wood[i].tex = wood.Vertices[i].tex;

	}
	const UINT vbByteSize_wood = (UINT)vertices_wood.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices_wood = wood.getIndices16();
	const UINT ibByteSize_wood = (UINT)indices_wood.size() * sizeof(std::uint16_t);

	auto geo_wood = std::make_unique<MeshGeometry>();
	geo_wood->Name = "wood";

	D3DCreateBlob(vbByteSize_wood, &geo_wood->VertexBufferCPU);
	CopyMemory(geo_wood->VertexBufferCPU->GetBufferPointer(), vertices_wood.data(), vbByteSize_wood);

	D3DCreateBlob(ibByteSize_wood, &geo_wood->IndexBufferCPU);
	CopyMemory(geo_wood->IndexBufferCPU->GetBufferPointer(), indices_wood.data(), ibByteSize_wood);

	geo_wood->VertexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), vertices_wood.data(), vbByteSize_wood, geo_wood->VertexBufferUpload);

	geo_wood->IndexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), indices_wood.data(), ibByteSize_wood, geo_wood->IndexBufferUpload);

	geo_wood->VertexBufferStride = sizeof(Vertex);
	geo_wood->VertexBufferByteSize = vbByteSize_wood;
	geo_wood->IndexBufferByteSize = ibByteSize_wood;
	geo_wood->IndexFormat = DXGI_FORMAT_R16_UINT;


	SubmeshGeometry submesh_wood;
	submesh_wood.IndexCount = (UINT)indices_wood.size();
	submesh_wood.StartIndexLocation = 0;
	submesh_wood.BaseVertexLocation = 0;

	geo_wood->DrawArgs["wood"] = submesh_wood;

	mGeometries["wood"] = std::move(geo_wood);

	
	std::vector<std::uint16_t> indices_fire(mFires["fire1"]->VertexCount());
	for (size_t i = 0; i < indices_fire.size(); i++)
	{
		indices_fire[i] = i;
	}
	
	UINT vbByteSize_fire = mFires["fire1"]->VertexCount() * sizeof(Vertex);
	UINT ibByteSize_fire = mFires["fire1"]->VertexCount() * sizeof(std::uint16_t);

	auto geo_fire = std::make_unique<MeshGeometry>();
	geo_fire->Name = "fire1";

	geo_fire->VertexBufferCPU = nullptr;
	geo_fire->VertexBufferGPU = nullptr;

	D3DCreateBlob(ibByteSize_fire, &geo_fire->IndexBufferCPU);
	CopyMemory(geo_fire->IndexBufferCPU->GetBufferPointer(),indices_fire.data(),ibByteSize_fire);

	geo_fire->IndexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(), 
		mCmdList.Get(), indices_fire.data(), ibByteSize_fire, geo_fire->IndexBufferUpload);

	geo_fire->VertexBufferStride = sizeof(Vertex);
	geo_fire->VertexBufferByteSize = vbByteSize_fire;
	geo_fire->IndexBufferByteSize = ibByteSize_fire;
	geo_fire->IndexFormat = DXGI_FORMAT_R16_UINT;

	SubmeshGeometry submesh_fire;
	submesh_fire.IndexCount = (UINT)indices_fire.size();
	submesh_fire.StartIndexLocation = 0;
	submesh_fire.BaseVertexLocation = 0;

	geo_fire->DrawArgs["fire"] = submesh_fire;

	mGeometries["fire"] = std::move(geo_fire);

	GeometryGenerator::MeshData_t haystack;
	mModelLoader.loadModel("haystack.FBX", haystack);

	std::vector<Vertex> vertices_haystack(haystack.Vertices.size());
	for (int i = 0; i < haystack.Vertices.size(); ++i)
	{
		vertices_haystack[i].pos = haystack.Vertices[i].pos;
		vertices_haystack[i].col = haystack.Vertices[i].col;
		vertices_haystack[i].nor = haystack.Vertices[i].nor;
		vertices_haystack[i].tex.x = haystack.Vertices[i].tex.x;
		vertices_haystack[i].tex.y = 1 - haystack.Vertices[i].tex.y;

	}
	const UINT vbByteSize_haystack = (UINT)vertices_haystack.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices_haystack = haystack.getIndices16();
	const UINT ibByteSize_haystack = (UINT)indices_haystack.size() * sizeof(std::uint16_t);

	auto geo_haystack = std::make_unique<MeshGeometry>();
	geo_haystack->Name = "haystack";

	D3DCreateBlob(vbByteSize_haystack, &geo_haystack->VertexBufferCPU);
	CopyMemory(geo_haystack->VertexBufferCPU->GetBufferPointer(), vertices_haystack.data(), vbByteSize_haystack);

	D3DCreateBlob(ibByteSize_haystack, &geo_haystack->IndexBufferCPU);
	CopyMemory(geo_haystack->IndexBufferCPU->GetBufferPointer(), indices_haystack.data(), ibByteSize_haystack);

	geo_haystack->VertexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), vertices_haystack.data(), vbByteSize_haystack, geo_haystack->VertexBufferUpload);

	geo_haystack->IndexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), indices_haystack.data(), ibByteSize_haystack, geo_haystack->IndexBufferUpload);

	geo_haystack->VertexBufferStride = sizeof(Vertex);
	geo_haystack->VertexBufferByteSize = vbByteSize_haystack;
	geo_haystack->IndexBufferByteSize = ibByteSize_haystack;
	geo_haystack->IndexFormat = DXGI_FORMAT_R16_UINT;


	SubmeshGeometry submesh_haystack;
	submesh_haystack.IndexCount = (UINT)indices_haystack.size();
	submesh_haystack.StartIndexLocation = 0;
	submesh_haystack.BaseVertexLocation = 0;

	geo_haystack->DrawArgs["haystack"] = submesh_haystack;

	mGeometries["haystack"] = std::move(geo_haystack);


	GeometryGenerator::MeshData_t church;
	mModelLoader.loadModel("church.FBX", church);

	std::vector<Vertex> vertices_church(church.Vertices.size());
	for (int i = 0; i < church.Vertices.size(); ++i)
	{
		vertices_church[i].pos = church.Vertices[i].pos;
		vertices_church[i].col = church.Vertices[i].col;
		vertices_church[i].nor = church.Vertices[i].nor;
		vertices_church[i].tex.x = church.Vertices[i].tex.x;
		vertices_church[i].tex.y = 1 - church.Vertices[i].tex.y;

	}
	const UINT vbByteSize_church = (UINT)vertices_church.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices_church = church.getIndices16();
	const UINT ibByteSize_church = (UINT)indices_church.size() * sizeof(std::uint16_t);

	auto geo_church = std::make_unique<MeshGeometry>();
	geo_church->Name = "church";

	D3DCreateBlob(vbByteSize_church, &geo_church->VertexBufferCPU);
	CopyMemory(geo_church->VertexBufferCPU->GetBufferPointer(), vertices_church.data(), vbByteSize_church);

	D3DCreateBlob(ibByteSize_church, &geo_church->IndexBufferCPU);
	CopyMemory(geo_church->IndexBufferCPU->GetBufferPointer(), indices_church.data(), ibByteSize_church);

	geo_church->VertexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), vertices_church.data(), vbByteSize_church, geo_church->VertexBufferUpload);

	geo_church->IndexBufferGPU = DXUtil::createDefaultBuffer(mDevice.Get(),
		mCmdList.Get(), indices_church.data(), ibByteSize_church, geo_church->IndexBufferUpload);

	geo_church->VertexBufferStride = sizeof(Vertex);
	geo_church->VertexBufferByteSize = vbByteSize_church;
	geo_church->IndexBufferByteSize = ibByteSize_church;
	geo_church->IndexFormat = DXGI_FORMAT_R16_UINT;


	SubmeshGeometry submesh_church;
	submesh_church.IndexCount = (UINT)indices_church.size();
	submesh_church.StartIndexLocation = 0;
	submesh_church.BaseVertexLocation = 0;

	geo_church->DrawArgs["church"] = submesh_church;

	mGeometries["church"] = std::move(geo_church);

}


void Campfire::buildRenderItem()
{
	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixScaling(100.0f, 1.02f, 100.0f));
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(500.0f, 500.0f, 100.0f));
	gridRitem->Geo = mGeometries["camp"].get();
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->objCBIndex = 0;
	gridRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());

	mAllRitems.push_back(std::move(gridRitem));

	auto skyRitem = std::make_unique<RenderItem>();
	skyRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&skyRitem->World, XMMatrixScaling(0.15f, 0.2f, 0.15f));
	XMStoreFloat4x4(&skyRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	skyRitem->Geo = mGeometries["sky"].get();
	skyRitem->Mat = mMaterials["sky"].get();
	skyRitem->objCBIndex = 1;
	skyRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skyRitem->BaseVertexLocation = skyRitem->Geo->DrawArgs["sky"].BaseVertexLocation;
	skyRitem->StartIndexLocation = skyRitem->Geo->DrawArgs["sky"].StartIndexLocation;
	skyRitem->IndexCount = skyRitem->Geo->DrawArgs["sky"].IndexCount;
	mRitemLayer[(int)RenderLayer::Sky].push_back(skyRitem.get());

	mAllRitems.push_back(std::move(skyRitem));

	auto campRitem = std::make_unique<RenderItem>();
	campRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&campRitem->World,XMMatrixScaling(5.0f, 5.0f, 5.0f));
	XMStoreFloat4x4(&campRitem->TexTransform, XMMatrixScaling(1.0f, 0.5f, 1.0f));
	campRitem->Geo = mGeometries["wood"].get();
	campRitem->Mat = mMaterials["camp"].get();
	campRitem->objCBIndex = 2;
	campRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	campRitem->BaseVertexLocation = campRitem->Geo->DrawArgs["wood"].BaseVertexLocation;
	campRitem->StartIndexLocation = campRitem->Geo->DrawArgs["wood"].StartIndexLocation;
	campRitem->IndexCount = campRitem->Geo->DrawArgs["wood"].IndexCount;
	mRitemLayer[(int)RenderLayer::Camp].push_back(campRitem.get());

	mAllRitems.push_back(std::move(campRitem));

	auto fireRitem = std::make_unique<RenderItem>();
	fireRitem->World = MathHelper::Identity4x4();
	fireRitem->TexTransform = MathHelper::Identity4x4();
	fireRitem->Geo = mGeometries["fire"].get();
	fireRitem->Mat = mMaterials["fire"].get();
	fireRitem->objCBIndex = 3;
	fireRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	fireRitem->BaseVertexLocation = fireRitem->Geo->DrawArgs["fire"].BaseVertexLocation;
	fireRitem->StartIndexLocation = fireRitem->Geo->DrawArgs["fire"].StartIndexLocation;
	fireRitem->IndexCount = fireRitem->Geo->DrawArgs["fire"].IndexCount;
	mRitemLayer[(int)RenderLayer::Fire].push_back(fireRitem.get());

	mDynamicRitems["fire1"] = fireRitem.get();
	mAllRitems.push_back(std::move(fireRitem));

	auto haystack1Ritem = std::make_unique<RenderItem>();
	haystack1Ritem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&haystack1Ritem->World, XMMatrixTranslation(25.0f, 0.0f, 15.0f) * XMMatrixScaling(10.0f, 10.0f, 10.0f));
	haystack1Ritem->TexTransform = MathHelper::Identity4x4();
	haystack1Ritem->Geo = mGeometries["haystack"].get();
	haystack1Ritem->Mat = mMaterials["haystack"].get();
	haystack1Ritem->objCBIndex = 4;
	haystack1Ritem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	haystack1Ritem->BaseVertexLocation = haystack1Ritem->Geo->DrawArgs["haystack"].BaseVertexLocation;
	haystack1Ritem->StartIndexLocation = haystack1Ritem->Geo->DrawArgs["haystack"].StartIndexLocation;
	haystack1Ritem->IndexCount = haystack1Ritem->Geo->DrawArgs["haystack"].IndexCount;
	mRitemLayer[(int)RenderLayer::Haystack1].push_back(haystack1Ritem.get());

	mAllRitems.push_back(std::move(haystack1Ritem));

	auto haystack2Ritem = std::make_unique<RenderItem>();
	haystack2Ritem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&haystack2Ritem->World, XMMatrixTranslation(25.0f, 0.0f, 25.0f) * XMMatrixScaling(10.0f, 10.0f, 10.0f));
	haystack2Ritem->TexTransform = MathHelper::Identity4x4();
	haystack2Ritem->Geo = mGeometries["haystack"].get();
	haystack2Ritem->Mat = mMaterials["haystack"].get();
	haystack2Ritem->objCBIndex = 5;
	haystack2Ritem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	haystack2Ritem->BaseVertexLocation = haystack2Ritem->Geo->DrawArgs["haystack"].BaseVertexLocation;
	haystack2Ritem->StartIndexLocation = haystack2Ritem->Geo->DrawArgs["haystack"].StartIndexLocation;
	haystack2Ritem->IndexCount = haystack2Ritem->Geo->DrawArgs["haystack"].IndexCount;
	mRitemLayer[(int)RenderLayer::Haystack2].push_back(haystack2Ritem.get());

	mAllRitems.push_back(std::move(haystack2Ritem));

	auto churchRitem = std::make_unique<RenderItem>();
	churchRitem->World = MathHelper::Identity4x4();
	XMStoreFloat4x4(&churchRitem->World, XMMatrixTranslation(-50.0f, 0.0f, 50.0f) * XMMatrixScaling(10.0f, 10.0f, 10.0f));
	churchRitem->TexTransform = MathHelper::Identity4x4();
	churchRitem->Geo = mGeometries["church"].get();
	churchRitem->Mat = mMaterials["church"].get();
	churchRitem->objCBIndex = 6;
	churchRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	churchRitem->BaseVertexLocation = churchRitem->Geo->DrawArgs["church"].BaseVertexLocation;
	churchRitem->StartIndexLocation = churchRitem->Geo->DrawArgs["church"].StartIndexLocation;
	churchRitem->IndexCount = churchRitem->Geo->DrawArgs["church"].IndexCount;
	mRitemLayer[(int)RenderLayer::Church].push_back(churchRitem.get());

	mAllRitems.push_back(std::move(churchRitem));

}

void Campfire::buildFrameResource()
{
	for (UINT i = 0; i < 3; ++i)
	{
		auto frameResource = std::make_unique<FrameResource>(mDevice.Get(), 1, (UINT)mAllRitems.size(), (UINT)mMaterials.size());
		frameResource.get()->addDynamicElements("fire1", mFires["fire1"]->VertexCount());
		mFrameResources.push_back(std::move(frameResource));
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
	opaquePSO.pRootSignature = mTerranRootSignature.Get();
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


	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPSO;
	ZeroMemory(&skyPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	skyPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	skyPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	skyPSO.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	skyPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	skyPSO.SampleMask = UINT_MAX;
	skyPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	skyPSO.NumRenderTargets = 1;
	skyPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	skyPSO.pRootSignature = mModelRootSignature["sky"].Get();
	skyPSO.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	skyPSO.SampleDesc.Count = 1;
	skyPSO.SampleDesc.Quality = 0;
	skyPSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	skyPSO.VS =
	{
		reinterpret_cast<BYTE *>(mShaders["skyVS"]->GetBufferPointer()),
		mShaders["skyVS"]->GetBufferSize()
	};
	skyPSO.PS =
	{
		reinterpret_cast<BYTE *>(mShaders["skyPS"]->GetBufferPointer()),
		mShaders["skyPS"]->GetBufferSize()
	};
	mDevice->CreateGraphicsPipelineState(&skyPSO, IID_PPV_ARGS(mPSOs["sky"].GetAddressOf()));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC campPSO;
	ZeroMemory(&campPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	campPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	campPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	campPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	campPSO.SampleMask = UINT_MAX;
	campPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	campPSO.NumRenderTargets = 1;
	campPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	campPSO.pRootSignature = mModelRootSignature["camp"].Get();
	campPSO.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	campPSO.SampleDesc.Count = 1;
	campPSO.SampleDesc.Quality = 0;
	campPSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	campPSO.VS =
	{
		reinterpret_cast<BYTE *>(mShaders["campVS"]->GetBufferPointer()),
		mShaders["campVS"]->GetBufferSize()
	};
	campPSO.PS =
	{
		reinterpret_cast<BYTE *>(mShaders["campPS"]->GetBufferPointer()),
		mShaders["campPS"]->GetBufferSize()
	};
	mDevice->CreateGraphicsPipelineState(&campPSO, IID_PPV_ARGS(mPSOs["camp"].GetAddressOf()));

	D3D12_RENDER_TARGET_BLEND_DESC fireBlendDesc;
	fireBlendDesc.BlendEnable = true;
	fireBlendDesc.LogicOpEnable = false;
	fireBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	fireBlendDesc.DestBlend = D3D12_BLEND_ONE;
	fireBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	fireBlendDesc.SrcBlendAlpha = D3D12_BLEND_ZERO;
	fireBlendDesc.DestBlendAlpha = D3D12_BLEND_ONE;
	fireBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_MAX;
	fireBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	fireBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC firePSO;
	ZeroMemory(&firePSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	firePSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	firePSO.BlendState.RenderTarget[0] = fireBlendDesc;
	firePSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	firePSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	firePSO.SampleMask = UINT_MAX;
	firePSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	firePSO.NumRenderTargets = 1;
	firePSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	firePSO.pRootSignature = mEffectRootSignature["fire1"].Get();
	firePSO.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	firePSO.SampleDesc.Count = 1;
	firePSO.SampleDesc.Quality = 0;
	firePSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	firePSO.VS =
	{
		reinterpret_cast<BYTE *>(mShaders["fireVS"]->GetBufferPointer()),
		mShaders["fireVS"]->GetBufferSize()
	};
	firePSO.GS =
	{
		reinterpret_cast<BYTE *>(mShaders["fireGS"]->GetBufferPointer()),
		mShaders["fireGS"]->GetBufferSize()
	};
	firePSO.PS =
	{
		reinterpret_cast<BYTE *>(mShaders["firePS"]->GetBufferPointer()),
		mShaders["firePS"]->GetBufferSize()
	};
	mDevice->CreateGraphicsPipelineState(&firePSO, IID_PPV_ARGS(mPSOs["fire1"].GetAddressOf()));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC haystackPSO;
	ZeroMemory(&haystackPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	haystackPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	haystackPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	haystackPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	haystackPSO.SampleMask = UINT_MAX;
	haystackPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	haystackPSO.NumRenderTargets = 1;
	haystackPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	haystackPSO.pRootSignature = mModelRootSignature["haystack"].Get();
	haystackPSO.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	haystackPSO.SampleDesc.Count = 1;
	haystackPSO.SampleDesc.Quality = 0;
	haystackPSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	haystackPSO.VS =
	{
		reinterpret_cast<BYTE *>(mShaders["haystackVS"]->GetBufferPointer()),
		mShaders["haystackVS"]->GetBufferSize()
	};
	haystackPSO.PS =
	{
		reinterpret_cast<BYTE *>(mShaders["haystackPS"]->GetBufferPointer()),
		mShaders["haystackPS"]->GetBufferSize()
	};
	mDevice->CreateGraphicsPipelineState(&haystackPSO, IID_PPV_ARGS(mPSOs["haystack"].GetAddressOf()));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC churchPSO;
	ZeroMemory(&churchPSO, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	churchPSO.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	churchPSO.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	churchPSO.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	churchPSO.SampleMask = UINT_MAX;
	churchPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	churchPSO.NumRenderTargets = 1;
	churchPSO.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	churchPSO.pRootSignature = mModelRootSignature["church"].Get();
	churchPSO.InputLayout = { mInputLayout.data(),(UINT)mInputLayout.size() };
	churchPSO.SampleDesc.Count = 1;
	churchPSO.SampleDesc.Quality = 0;
	churchPSO.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	churchPSO.VS =
	{
		reinterpret_cast<BYTE *>(mShaders["churchVS"]->GetBufferPointer()),
		mShaders["churchVS"]->GetBufferSize()
	};
	churchPSO.PS =
	{
		reinterpret_cast<BYTE *>(mShaders["churchPS"]->GetBufferPointer()),
		mShaders["churchPS"]->GetBufferSize()
	};
	mDevice->CreateGraphicsPipelineState(&churchPSO, IID_PPV_ARGS(mPSOs["church"].GetAddressOf()));

	

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
		cmdList->IASetPrimitiveTopology(ri->primitiveType);

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

void Campfire::updateDynamicElements(const GameTimer & gt)
{
	static float t_base = 0.0f; 
	mFires["fire1"]->Update(gt.DeltaTime());
	auto currfire1VB = mCurrFrameResource->dynamicElement["fire1"].get();
	for (int i = 0; i < mFires["fire1"]->VertexCount(); ++i)
	{
		Vertex v;

		v.pos = mFires["fire1"]->Position(i).position;
		v.col = mFires["fire1"]->Position(i).color;
		v.nor = XMFLOAT3(1.0f, 1.0f, 1.0f);
		v.tex = XMFLOAT2(1.0f, 1.0f);

		currfire1VB->CopyData(i, v);
	}

	mDynamicRitems["fire1"]->Geo->VertexBufferGPU = currfire1VB->getUploadBuffer();

	
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




