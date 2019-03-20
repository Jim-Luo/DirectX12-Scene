#include "stdafx.h"
#include "ModelLoader.h"


void ModelLoader::processNode(FbxNode * pNode)
{
	FbxNodeAttribute::EType AttributeType;
	if (pNode->GetNodeAttribute())
	{
		switch (pNode->GetNodeAttribute()->GetAttributeType())
		{
		case  FbxNodeAttribute::eMesh:
			processMesh(pNode);
			break;
		default:
			break;
		}
	}
	for (int i = 0; i < pNode->GetChildCount(); ++i)
	{
		processNode(pNode->GetChild(i));
	}
}

void ModelLoader::processMesh(FbxNode * pNode)
{
	FbxMesh *pMesh = pNode->GetMesh();
	if (pMesh == nullptr)
		return;

	UINT triangleCount = pMesh->GetPolygonCount();
	UINT vertexCount = 0;

	vertices.resize(triangleCount * 3);
	indices.resize(triangleCount * 3);
	for (int i = 0; i < triangleCount; ++i)
	{
		for (int j = 0; j < 3; j++)
		{
			indices[vertexCount] = i * 3 + j;
			mVertex vertex;
			int ctrlPointIndex = pMesh->GetPolygonVertex(i, j);

			// Read the vertex 
			FbxVector4* pCtrlPoint = pMesh->GetControlPoints();


			vertex.pos.x = pCtrlPoint[ctrlPointIndex].mData[0];
			vertex.pos.y = pCtrlPoint[ctrlPointIndex].mData[1];
			vertex.pos.z = pCtrlPoint[ctrlPointIndex].mData[2];

			// Read the color of each vertex
			if (pMesh->GetElementVertexColorCount() < 1)
			{
				vertex.col = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			}
			else
			{
				FbxGeometryElementVertexColor* pVertexColor = pMesh->GetElementVertexColor(0);
				//pMesh->GetElementMaterial
				switch (pVertexColor->GetMappingMode())
				{
			         case FbxGeometryElement::eByControlPoint:
			         {
						 switch (pVertexColor->GetReferenceMode())
						 {
						 case FbxGeometryElement::eDirect:
						 {
							 vertex.col.x = pVertexColor->GetDirectArray().GetAt(ctrlPointIndex).mRed;
							 vertex.col.y = pVertexColor->GetDirectArray().GetAt(ctrlPointIndex).mGreen;
							 vertex.col.z = pVertexColor->GetDirectArray().GetAt(ctrlPointIndex).mBlue;
							 vertex.col.w = pVertexColor->GetDirectArray().GetAt(ctrlPointIndex).mAlpha;
						 }
						 break;
                          case FbxGeometryElement::eIndexToDirect:
						 {
							 int id = pVertexColor->GetIndexArray().GetAt(ctrlPointIndex);
							 vertex.col.x = pVertexColor->GetDirectArray().GetAt(id).mRed;
							 vertex.col.y = pVertexColor->GetDirectArray().GetAt(id).mGreen;
							 vertex.col.z = pVertexColor->GetDirectArray().GetAt(id).mBlue;
							 vertex.col.w = pVertexColor->GetDirectArray().GetAt(id).mAlpha;
						  }
						 break;
						  default:
							  break;
						 }
					 }
					 break;
					 case FbxGeometryElement::eByPolygonVertex:
					 {
						 switch (pVertexColor->GetReferenceMode())
						 {
						 case FbxGeometryElement::eDirect:
						 {
							 vertex.col.x = pVertexColor->GetDirectArray().GetAt(vertexCount).mRed;
							 vertex.col.y = pVertexColor->GetDirectArray().GetAt(vertexCount).mGreen;
							 vertex.col.z = pVertexColor->GetDirectArray().GetAt(vertexCount).mBlue;
							 vertex.col.w = pVertexColor->GetDirectArray().GetAt(vertexCount).mAlpha;
						 }
						 break;
						 case FbxGeometryElement::eIndexToDirect:
						 {
							 int id = pVertexColor->GetIndexArray().GetAt(vertexCount);
							 vertex.col.x = pVertexColor->GetDirectArray().GetAt(id).mRed;
							 vertex.col.y = pVertexColor->GetDirectArray().GetAt(id).mGreen;
							 vertex.col.z = pVertexColor->GetDirectArray().GetAt(id).mBlue;
							 vertex.col.w = pVertexColor->GetDirectArray().GetAt(id).mAlpha;
						 }
						 break;
						 default:
							 break;
						 }
					 }
					 break;
				}
			}

			
			// Read the texture of each vector
			for (int k = 0; k < 2; ++k)
			{

				ReadUV(pMesh, pMesh->GetPolygonVertex(i, j), pMesh->GetTextureUVIndex(i, j), k, &vertex.tex);

				/*if (pMesh->GetElementUVCount() <= k)
				{
					vertex.tex.x = 1.0f;
					vertex.tex.y = 1.0f;
				}
				else
				{
					FbxGeometryElementUV* pVertexUV = pMesh->GetElementUV(k);

					switch (pVertexUV->GetMappingMode())
					{
					case FbxGeometryElement::eByControlPoint:
					{
						switch (pVertexUV->GetReferenceMode())
						{
						case FbxGeometryElement::eDirect:
						{
							vertex.tex.x = pVertexUV->GetDirectArray().GetAt(ctrlPointIndex).mData[0];
							vertex.tex.y = pVertexUV->GetDirectArray().GetAt(ctrlPointIndex).mData[1];
						}
						break;

						case FbxGeometryElement::eIndexToDirect:
						{
							int id = pVertexUV->GetIndexArray().GetAt(ctrlPointIndex);
							vertex.tex.x = pVertexUV->GetDirectArray().GetAt(id).mData[0];
							vertex.tex.y = pVertexUV->GetDirectArray().GetAt(id).mData[1];
						}
						break;

						default:
							break;
						}
					}
					break;

					case FbxGeometryElement::eByPolygonVertex:
					{
						switch (pVertexUV->GetReferenceMode())
						{
						case FbxGeometryElement::eDirect:
						case FbxGeometryElement::eIndexToDirect:
						{
							vertex.tex.x = pVertexUV->GetDirectArray().GetAt(pMesh->GetTextureUVIndex(i, j)).mData[0];
							vertex.tex.y = pVertexUV->GetDirectArray().GetAt(pMesh->GetTextureUVIndex(i, j)).mData[1];
						}
						break;

						default:
							break;
						}
					}
					break;
					}
				}	*/
			}


			// Read the normal of each vertex 

			FbxGeometryElementNormal* leNormal = pMesh->GetElementNormal(0);
			switch (leNormal->GetMappingMode())
			{
			case FbxGeometryElement::eByControlPoint:
			{
				switch (leNormal->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					vertex.norm.x = leNormal->GetDirectArray().GetAt(ctrlPointIndex).mData[0];
					vertex.norm.y = leNormal->GetDirectArray().GetAt(ctrlPointIndex).mData[1];
					vertex.norm.z = leNormal->GetDirectArray().GetAt(ctrlPointIndex).mData[2];
					vertex.norm.z = 1.0f;
				}
				break;

				case FbxGeometryElement::eIndexToDirect:
				{
					int id = leNormal->GetIndexArray().GetAt(ctrlPointIndex);
					vertex.norm.x = leNormal->GetDirectArray().GetAt(id).mData[0];
					vertex.norm.y = leNormal->GetDirectArray().GetAt(id).mData[1];
					vertex.norm.z = leNormal->GetDirectArray().GetAt(id).mData[2];
					vertex.norm.z = 1.0f;
				}
				break;

				default:
					break;
				}
			}
			break;

			case FbxGeometryElement::eByPolygonVertex:
			{
				switch (leNormal->GetReferenceMode())
				{
				case FbxGeometryElement::eDirect:
				{
					vertex.norm.x = leNormal->GetDirectArray().GetAt(ctrlPointIndex).mData[0];
					vertex.norm.y = leNormal->GetDirectArray().GetAt(ctrlPointIndex).mData[1];
					vertex.norm.z = leNormal->GetDirectArray().GetAt(ctrlPointIndex).mData[2];
					vertex.norm.z = 1.0f;
				}
				break;

				case FbxGeometryElement::eIndexToDirect:
				{
					int id = leNormal->GetIndexArray().GetAt(ctrlPointIndex);
					vertex.norm.x = leNormal->GetDirectArray().GetAt(id).mData[0];
					vertex.norm.y = leNormal->GetDirectArray().GetAt(id).mData[1];
					vertex.norm.z = leNormal->GetDirectArray().GetAt(id).mData[2];
					vertex.norm.z = 1.0f;
				}
				break;

				default:
					break;
				}
			}
			break;
			}
			vertices[vertexCount] = vertex;
			vertexCount++;
		}

		// 根据读入的信息组装三角形，并以某种方式使用即可，比如存入到列表中、保存到文件等... 
	}
}

void ModelLoader::ReadUV(FbxMesh* pMesh, int ctrlPointIndex, int textureUVIndex, int uvLayer, XMFLOAT2* pUV)
{
	if (uvLayer >= 2 || pMesh->GetElementUVCount() <= uvLayer)
	{
		return;
	}

	FbxGeometryElementUV* pVertexUV = pMesh->GetElementUV(uvLayer);

	switch (pVertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
	{
		switch (pVertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			pUV->x = pVertexUV->GetDirectArray().GetAt(ctrlPointIndex).mData[0];
			pUV->y = pVertexUV->GetDirectArray().GetAt(ctrlPointIndex).mData[1];
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int id = pVertexUV->GetIndexArray().GetAt(ctrlPointIndex);
			pUV->x = pVertexUV->GetDirectArray().GetAt(id).mData[0];
			pUV->y = pVertexUV->GetDirectArray().GetAt(id).mData[1];
		}
		break;

		default:
			break;
		}
	}
	break;

	case FbxGeometryElement::eByPolygonVertex:
	{
		switch (pVertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		case FbxGeometryElement::eIndexToDirect:
		{
			pUV->x = pVertexUV->GetDirectArray().GetAt(textureUVIndex).mData[0];
			pUV->y = pVertexUV->GetDirectArray().GetAt(textureUVIndex).mData[1];
		}
		break;

		default:
			break;
		}
	}
	break;
	}
}


HRESULT ModelLoader::loadModel(const char * filename, GeometryGenerator::MeshData& meshData)
{
	bool bSuccess = mFbxImporter->Initialize(filename, -1, mFbxSdkManager->GetIOSettings());
	if (!bSuccess) return E_FAIL;

	bSuccess = mFbxImporter->Import(mFbxScene);
	if (!bSuccess) return E_FAIL;


	FbxNode* pFbxRootNode = mFbxScene->GetRootNode();
	processNode(pFbxRootNode);

	meshData.Vertices.resize(vertices.size());
	meshData.Indices32.resize(indices.size());
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		meshData.Vertices[i].pos = vertices[i].pos;
		meshData.Indices32[i] = indices[i];
	}

	return S_OK;

}

HRESULT ModelLoader::loadModel(const char * filename, GeometryGenerator::MeshData_t & meshData_t)
{
	bool bSuccess = mFbxImporter->Initialize(filename, -1, mFbxSdkManager->GetIOSettings());
	if (!bSuccess) return E_FAIL;

	bSuccess = mFbxImporter->Import(mFbxScene);
	if (!bSuccess) return E_FAIL;


	FbxNode* pFbxRootNode = mFbxScene->GetRootNode();
	processNode(pFbxRootNode);

	meshData_t.Vertices.resize(vertices.size());
	meshData_t.Indices32.resize(indices.size());
	for (size_t i = 0; i < vertices.size(); i++)
	{
		meshData_t.Vertices[i] = GeometryGenerator::Vertex_t(vertices[i].pos.x, vertices[i].pos.y, vertices[i].pos.z, 
			vertices[i].norm.x, vertices[i].norm.y, vertices[i].norm.z,
			vertices[i].col.x, vertices[i].col.y, vertices[i].col.z, vertices[i].col.w,
			vertices[i].tex.x,vertices[i].tex.y);
		meshData_t.Indices32[i] = indices[i];
	}


	return S_OK;
}

HRESULT ModelLoader::loadModel(const char * filename, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices)
{
	bool bSuccess = mFbxImporter->Initialize(filename, -1, mFbxSdkManager->GetIOSettings());
	if (!bSuccess) return E_FAIL;

	bSuccess = mFbxImporter->Import(mFbxScene);
	if (!bSuccess) return E_FAIL;


	FbxNode* pFbxRootNode = mFbxScene->GetRootNode();
	processNode(pFbxRootNode);


	return E_NOTIMPL;
}


ModelLoader::ModelLoader()
{
	mFbxSdkManager = FbxManager::Create();
	mFbxIOsettings = FbxIOSettings::Create(mFbxSdkManager, IOSROOT);
	mFbxSdkManager->SetIOSettings(mFbxIOsettings);
	mFbxImporter = FbxImporter::Create(mFbxSdkManager, "main_importer");
	mFbxScene = FbxScene::Create(mFbxSdkManager, "main_scene");
}


ModelLoader::~ModelLoader()
{
	mFbxImporter->Destroy();
	mFbxScene->Destroy();
	mFbxIOsettings->Destroy();
	mFbxSdkManager->Destroy();
}
