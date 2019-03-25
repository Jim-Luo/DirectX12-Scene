#pragma once
#include "DXSample.h"
#include "DXUtil.h"
#include "Camera.h"
#include "ModelLoader.h"
#include "Fire.h"

class Campfire :
	public DXSample
{
private:
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	Camera mCamera;

	ModelLoader mModelLoader;

	
protected:
	ComPtr<ID3D12RootSignature> mTerranRootSignature;
	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> mModelRootSignature;
	std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> mEffectRootSignature;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	PassConstants mMainPassCB;

	std::unordered_map<std::string, std::unique_ptr<Fire>> mFires;
	std::unordered_map<std::string, RenderItem*> mDynamicRitems;
	
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
public:
	virtual bool initDirect3D();

	virtual void Resize();
	virtual void Update(const GameTimer& gt);
	virtual void Draw();

	virtual void OnLMouseDown(WPARAM btnState, int x, int y);
	virtual void OnLMouseUp(WPARAM btnState, int x, int y);
	virtual void OnRMouseDown(WPARAM btnState, int x, int y);
	virtual void OnRMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	virtual void OnKeyboardInput(const GameTimer &gt);
	
	void loadTexture();
	void buildDescriptorHeaps();
	void buildRootSignature();
	void buildShaderAndInputLayout();
	void buildMaterials();
	void buildGeometry();
	void buildRenderItem();
	void buildFrameResource();
	void buildPSO();
	void drawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	void updateDynamicElements(const GameTimer& gt);

	XMFLOAT3 getHillsNormal(float x, float z)const;
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	
public:
	Campfire();
	~Campfire();
};

