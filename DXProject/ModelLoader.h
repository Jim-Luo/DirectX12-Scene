#pragma once

#include "DXUtil.h"

class ModelLoader
{

private:
	struct mVertex
	{
		XMFLOAT3 pos;
		XMFLOAT4 col;
		XMFLOAT4 norm;
		XMFLOAT2 tex;
	};
	std::vector<mVertex> vertices;
	std::vector<int32_t> indices;
	FbxManager* mFbxSdkManager = nullptr;
	FbxIOSettings* mFbxIOsettings = nullptr;
	FbxImporter* mFbxImporter = nullptr;
	FbxScene* mFbxScene = nullptr;

	void processNode(FbxNode *pNode);
	void processMesh(FbxNode *pNode);
	void ReadUV(FbxMesh* pMesh, int ctrlPointIndex, int textureUVIndex, int uvLayer, XMFLOAT2* pUV);
public:
	HRESULT loadModel(const char * filename, GeometryGenerator::MeshData & meshData);
	HRESULT loadModel(const char * filename, GeometryGenerator::MeshData_t & meshData_t);
	HRESULT loadModel(const char * filename, std::vector<Vertex> &vertices, std::vector<std::uint16_t> &indices);
	
	ModelLoader();
	~ModelLoader();
};

