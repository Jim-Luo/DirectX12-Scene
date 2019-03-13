#pragma once
#include "DXUtil.h"
#define RATIO static_cast<float>(mClientWidth) / mClientHeight
class DXSample
{
protected:
	ComPtr<IDXGIFactory4> mFactory;
	ComPtr<ID3D12Device> mDevice;
	ComPtr<ID3D12Fence> mFence;
	ComPtr<IDXGISwapChain> mSwapChain;
	ComPtr<ID3D12CommandAllocator> mCmdAlloc;
	ComPtr<ID3D12CommandQueue> mCmdQueue;
	ComPtr<ID3D12GraphicsCommandList> mCmdList;

	ComPtr<ID3D12DescriptorHeap> mRTVHeap;
	ComPtr<ID3D12DescriptorHeap> mDSVHeap;

	ComPtr<ID3D12Resource> mSwapChainBuffer[2];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	

	UINT64 mCurrentFence = 0;
	UINT mCurrentBackBuffer = 0;
	UINT mRTVDescriptorSize;
	UINT mDSVDescriptorSize;
	UINT mCBVSRVUAVDescriptorSize;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mRect;

	GameTimer mTimer;
public:
	virtual bool initDirect3D();
	D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView() const;
	D3D12_CPU_DESCRIPTOR_HANDLE getCurrentBackBufferView() const;
	ID3D12Resource* getCurrentBackBuffer() const;
	
	virtual void Resize();
	virtual void Update(const GameTimer& gt);
	virtual void Draw();
	virtual void Flush();

	virtual void OnLMouseUp(WPARAM btnState, int x, int y);
	virtual void OnLMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	virtual void OnRMouseDown(WPARAM btnState, int x, int y);
	virtual void OnRMouseUp(WPARAM btnState, int x, int y);
	virtual void OnKeyboardInput();
protected:
	static DXSample* mSample;
	HWND ghWnd = 0;

	int mClientWidth = 800;
	int mClientHeight = 600;
	POINT mLastMousePos;
	bool mIsMoving = false;
public:
	static DXSample* getSample();

	LRESULT CALLBACK wndProc(HWND hWnd,UINT msg,WPARAM wParam,LPARAM lParam);
	int Run();
	bool initWindows(HINSTANCE hInstance, int mCmdShow);

	DXSample();
	~DXSample();
};

