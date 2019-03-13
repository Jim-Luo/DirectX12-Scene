#include "stdafx.h"
#include "DXSample.h"


LRESULT CALLBACK WndProc(HWND hWnd, UINT msg,WPARAM wParam, LPARAM lParam)
{
	return DXSample::getSample()->wndProc(hWnd, msg, wParam,lParam);
}
DXSample* DXSample::mSample = nullptr;
DXSample* DXSample::getSample()
{
	return mSample;
}

LRESULT DXSample::wndProc(HWND hWnd, UINT msg,  WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
		{
			DestroyWindow(ghWnd);
		}
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (mDevice)
			Resize();
		return 0;
	case WM_LBUTTONDOWN:
		OnLMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
		OnLMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_RBUTTONDOWN:
		OnRMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_RBUTTONUP:
		OnRMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}
int DXSample::Run()
{
	MSG msg = {0};
	mTimer.Reset();
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mTimer.Tick();
			Update(mTimer);
			Draw();
		}
	}
	return (UINT)msg.wParam;
}
bool DXSample::initWindows(HINSTANCE hInstance, int mCmdShow)
{
	WNDCLASS wc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"basicWindow";
	wc.lpszMenuName = 0;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"Register ERROR", 0, 0);
		return false;
	}
	ghWnd = CreateWindow(L"basicWindow",
		L"test",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		hInstance,
		0);
	if (ghWnd == 0)
	{
		MessageBox(0, L"Create ERROR", 0, 0);
		return false;
	}

	ShowWindow(ghWnd, mCmdShow);
	UpdateWindow(ghWnd);

	return true;




}

bool DXSample::initDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)
	ComPtr<ID3D12Debug> mDebug;
	D3D12GetDebugInterface(IID_PPV_ARGS(mDebug.GetAddressOf()));
	mDebug->EnableDebugLayer();
#endif
	//build Factory1
	CreateDXGIFactory1(IID_PPV_ARGS(mFactory.GetAddressOf()));

	//build Device
	HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(mDevice.GetAddressOf()));
	if (FAILED(hr))
	{
		ComPtr<IDXGIAdapter> mAdapter;
		mFactory->EnumWarpAdapter(IID_PPV_ARGS(mAdapter.GetAddressOf()));
		D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(mDevice.GetAddressOf()));
	}

	//calculate Descriptor Size
	mRTVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	mDSVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	mCBVSRVUAVDescriptorSize = mDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	//build Fence
	mDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(mFence.GetAddressOf()));

	//build Command Object
    mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(mCmdAlloc.GetAddressOf()));
	D3D12_COMMAND_QUEUE_DESC cqdesc = {};
	cqdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cqdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	mDevice->CreateCommandQueue(&cqdesc, IID_PPV_ARGS(mCmdQueue.GetAddressOf()));
	mDevice->CreateCommandList(0x1, D3D12_COMMAND_LIST_TYPE_DIRECT, mCmdAlloc.Get(), nullptr, IID_PPV_ARGS(mCmdList.GetAddressOf()));
	mCmdList->Close();


	//build Swapchain
	DXGI_SWAP_CHAIN_DESC scdesc;
	scdesc.BufferCount = 2;
	scdesc.BufferDesc.Width = mClientWidth;
	scdesc.BufferDesc.Height = mClientHeight;
	scdesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scdesc.BufferDesc.RefreshRate.Numerator = 60;
	scdesc.BufferDesc.RefreshRate.Denominator = 1;
	scdesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	scdesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	scdesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scdesc.SampleDesc.Count = 1;
	scdesc.SampleDesc.Quality = 0;
	scdesc.OutputWindow = ghWnd;
	scdesc.Windowed = true;
	scdesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scdesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	mFactory->CreateSwapChain(mCmdQueue.Get(), &scdesc, mSwapChain.GetAddressOf());

	//build Descriptor Heap
	D3D12_DESCRIPTOR_HEAP_DESC rtvdesc;
	rtvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvdesc.NodeMask = 0x1;
	rtvdesc.NumDescriptors = 2;
	rtvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	mDevice->CreateDescriptorHeap(&rtvdesc, IID_PPV_ARGS(mRTVHeap.GetAddressOf()));
	D3D12_DESCRIPTOR_HEAP_DESC dsvdesc;
	dsvdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvdesc.NodeMask = 0x1;
	dsvdesc.NumDescriptors = 1;
	dsvdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	mDevice->CreateDescriptorHeap(&dsvdesc, IID_PPV_ARGS(mDSVHeap.GetAddressOf()));


	Resize();
	return true;
}

DXSample::DXSample()
{
	mSample = this;
}


DXSample::~DXSample()
{
}

void DXSample::Draw()
{

}
void DXSample::Update(const GameTimer& gt)
{

}
void DXSample::Resize()
{
	assert(mCmdAlloc);
	assert(mCmdList);
	assert(mSwapChain);
	Flush();

	mCmdList->Reset(mCmdAlloc.Get(), nullptr);
	for (UINT i = 0; i < 2; i++)
	{
		mSwapChainBuffer[i].Reset();
	}
	mDepthStencilBuffer.Reset();
	mSwapChain->ResizeBuffers(2, mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);

	mCurrentBackBuffer = 0;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < 2; i++)
	{
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()));
		mDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, mRTVDescriptorSize);
	}


	D3D12_RESOURCE_DESC dsDesc;
	dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	dsDesc.Alignment = 0;
	dsDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsDesc.Height = mClientHeight;
	dsDesc.Width = mClientWidth;
	dsDesc.DepthOrArraySize = 1;
	dsDesc.MipLevels = 1;
	dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	dsDesc.SampleDesc.Count = 1;
	dsDesc.SampleDesc.Quality = 0;
	D3D12_CLEAR_VALUE cVal;
	cVal.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	cVal.DepthStencil.Depth = 1.0f;
	cVal.DepthStencil.Stencil = 0;
	mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&dsDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&cVal,
		IID_PPV_ARGS(mDepthStencilBuffer.GetAddressOf()));
	mDepthStencilBuffer->SetName(L"mDepthStencilBuffer");
	mDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(), nullptr, getDepthStencilView());

	mCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	mCmdList->Close();

	ID3D12CommandList* list[] = { mCmdList.Get() };
	mCmdQueue->ExecuteCommandLists(_countof(list), list);

	Flush();

	mViewport.Height = static_cast<float>(mClientHeight);
	mViewport.Width = static_cast<float>(mClientWidth);
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.MaxDepth = 1.0f;
	mViewport.MinDepth = 0.0f;

	mRect = { 0,0,mClientWidth,mClientHeight };





}
void DXSample::Flush()
{
	mCurrentFence++;

	mCmdQueue->Signal(mFence.Get(), mCurrentFence);
	if (mFence->GetCompletedValue() < mCurrentFence)
	{
		HANDLE h = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
		mFence->SetEventOnCompletion(mCurrentFence, h);
		WaitForSingleObject(h, INFINITE);
		CloseHandle(h);
	}
}

void DXSample::OnLMouseUp(WPARAM btnState, int x, int y)
{
}

void DXSample::OnLMouseDown(WPARAM btnState, int x, int y)
{
}

void DXSample::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void DXSample::OnRMouseDown(WPARAM btnState, int x, int y)
{
}

void DXSample::OnRMouseUp(WPARAM btnState, int x, int y)
{
	mIsMoving = false;
}

void DXSample::OnKeyboardInput()
{
}

D3D12_CPU_DESCRIPTOR_HANDLE DXSample::getDepthStencilView() const
{
	return mDSVHeap->GetCPUDescriptorHandleForHeapStart();
}
D3D12_CPU_DESCRIPTOR_HANDLE DXSample::getCurrentBackBufferView() const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRTVHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrentBackBuffer,
		mRTVDescriptorSize
	);
}
ID3D12Resource* DXSample::getCurrentBackBuffer() const
{
	return mSwapChainBuffer[mCurrentBackBuffer].Get();
}