#include <iostream>
#include <string>
#include <vector>

#include <fbxsdk.h>

#include "SUNMaths.h"
#include "SUNWriter.h"

using namespace std;

static vector<sun::Vertex>                  s_vertices;  // 정점
static vector<uint>                          s_indices;  // 인덱스
static unordered_map<sun::Vertex, uint> s_indexMapping;  // 정점+인덱스 맵핑

static sun::vec3* s_rawPositions;                        // 정점 위치

static String s_name, s_inputName, s_outputName;

void LoadNode(FbxNode* node);
bool ParseSave(FbxMesh* mesh);

void      ParseControlPoints(const FbxMesh* mesh);
sun::vec3 ParseNormal       (const FbxMesh* mesh, int controlPointIndex, int vertexCount);
sun::vec3 ParseBinormal     (const FbxMesh* mesh, int controlPointIndex, int vertexCount);
sun::vec3 ParseTangent      (      FbxMesh* mesh, int controlPointIndex, int vertexCount);
sun::vec2 ParseUV           (const FbxMesh* mesh, int controlPointIndex, int inTextureUVIndex);

void InsertVertex(const sun::vec3& position, const sun::vec3& normal, const sun::vec2& uv, const sun::vec3& binormal, const sun::vec3& tangent);

int main()
{
	cout << "FBX 파일명을 입력해주세요.: ";

	cin >> s_name;

	s_inputName  = s_name + ".fbx";
	s_outputName = s_name + ".sun";

	FbxManager*  manager  = FbxManager:: Create();
	FbxScene*      scene  = FbxScene::   Create(manager, "scene");
	FbxImporter* importer = FbxImporter::Create(manager, "");

	int format = -1;

	if (!manager->GetIOPluginRegistry()->DetectReaderFileFormat(s_inputName.c_str(), format))
		format = manager->GetIOPluginRegistry()->FindReaderIDByDescription("FBX binary (*.fbx)");

	if (importer->Initialize(s_inputName.c_str(), format))
	{
		cout << "■■■■■■■■■■■■■■■■■■■" << endl;
		std::cout << "Importing " << s_inputName << "..." << std::endl;
		cout << "■■■■■■■■■■■■■■■■■■■" << endl;
	}
	else
	{
		cout << "▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲" << endl;
		std::cout << s_inputName << " 파일을 열 수 없습니다." << std::endl;
		cout << "▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲" << endl;
		getchar();
		exit(1);
	}
	
	if (importer->Import(scene))
	{
		// 씬 내의 좌표축을 바꾼다.
		FbxAxisSystem::MayaYUp.ConvertScene(scene);

		// 씬 내에서 삼각형화 할 수 있는 모든 노드를 삼각형화 시킨다.
		FbxGeometryConverter geometryConverter(manager);
		geometryConverter.Triangulate(scene, true);

		LoadNode(scene->GetRootNode());
	}
	
	cout << "■■■■■■■■■■■■■■■■■■■" << endl;
	cout << "Complete Convert " << s_inputName << " To " << s_outputName << endl;
	cout << "■■■■■■■■■■■■■■■■■■■";

	getchar();
	getchar();

	return 1;
}






void LoadNode(FbxNode* node)
{
	FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();
	cout << "Node" << endl;

	if (nodeAttribute && nodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		FbxMesh* mesh = node->GetMesh();

		ParseSave(mesh);
	}

	const uint childCount = node->GetChildCount();

	// 재귀
	for (uint i = 0; i < childCount; ++i)
		LoadNode(node->GetChild(i));
}

bool ParseSave(FbxMesh* mesh)
{
	if (!mesh->GetNode())
		return false;

	uint triangleCount = mesh->GetPolygonCount();

	uint vertexCount = 0;

	ParseControlPoints(mesh);

	for (int triangle = 0; triangle < triangleCount; ++triangle)
	{
		sun::vec3 tanget;
		sun::vec3 binormal;
		sun::vec2 uv;


		for (uint i = 0; i < 3; ++i)
		{
			int controlPointIndex = mesh->GetPolygonVertex(triangle, i);

			sun::vec3& position = s_rawPositions[controlPointIndex];
			sun::vec3    normal = ParseNormal  (mesh, controlPointIndex, vertexCount);
			sun::vec3  binormal = ParseBinormal(mesh, controlPointIndex, vertexCount);
			sun::vec3   tangent = ParseTangent (mesh, controlPointIndex, vertexCount);
			sun::vec2        uv = ParseUV      (mesh, controlPointIndex, mesh->GetTextureUVIndex(triangle, i));

			uv.y = 1.0f - uv.y;
			//position.z = position.z * -1.0f;
			//normal.z = normal.z * -1.0f;

			InsertVertex(position, normal, uv, binormal, tangent);

			vertexCount++;
		}
	}

	sun::SUNWriter writer(s_name, s_vertices, s_indices);
	writer.Write(s_outputName);

	return true;
}

void ParseControlPoints(const FbxMesh* mesh)
{
	uint count = mesh->GetControlPointsCount();

	s_rawPositions = new sun::vec3[count];

	for (uint i = 0; i < count; ++i)
	{
		sun::vec3 position;
		position.x = static_cast<float>(mesh->GetControlPointAt(i).mData[0]);
		position.y = static_cast<float>(mesh->GetControlPointAt(i).mData[1]);
		position.z = static_cast<float>(mesh->GetControlPointAt(i).mData[2]);

		s_rawPositions[i] = position;
	}
}

sun::vec3 ParseNormal(const FbxMesh* mesh, int controlPointIndex, int vertexCount)
{
	if (mesh->GetElementNormalCount() < 1)
		return sun::vec3();

	sun::vec3 result;

	const FbxGeometryElementNormal* vertexNormal = mesh->GetElementNormal(0);

	switch (vertexNormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controlPointIndex).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controlPointIndex).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(controlPointIndex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(controlPointIndex);
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		default:
			return sun::vec3();
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexNormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertexCount).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertexCount).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(vertexCount).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexNormal->GetIndexArray().GetAt(vertexCount);
			result.x = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexNormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		default:
			return sun::vec3();
		}
		break;
	}

	return result;
}

sun::vec3 ParseBinormal(const FbxMesh* mesh, int controlPointIndex, int vertexCount)
{
	if (mesh->GetElementBinormalCount() < 1)
		return sun::vec3();

	sun::vec3 result;

	const FbxGeometryElementBinormal* vertexBinormal = mesh->GetElementBinormal(0);

	switch (vertexBinormal->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexBinormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controlPointIndex).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controlPointIndex).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(controlPointIndex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexBinormal->GetIndexArray().GetAt(controlPointIndex);
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		default:
			return sun::vec3();
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexBinormal->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertexCount).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertexCount).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(vertexCount).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexBinormal->GetIndexArray().GetAt(vertexCount);
			result.x = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexBinormal->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		default:
			return sun::vec3();
		}
		break;
	}

	return result;
}

sun::vec3 ParseTangent(FbxMesh* mesh, int controlPointIndex, int vertexCount)
{
	if (mesh->GetElementTangentCount() < 1)
		return sun::vec3();

	sun::vec3 result;

	FbxGeometryElementTangent* vertexTangent = mesh->GetElementTangent(0);

	switch (vertexTangent->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controlPointIndex).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controlPointIndex).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(controlPointIndex).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexTangent->GetIndexArray().GetAt(controlPointIndex);
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		default:
			return sun::vec3();
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexTangent->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertexCount).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertexCount).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(vertexCount).mData[2]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexTangent->GetIndexArray().GetAt(vertexCount);
			result.x = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[1]);
			result.z = static_cast<float>(vertexTangent->GetDirectArray().GetAt(index).mData[2]);
		}
		break;
		default:
			return sun::vec3();
		}
		break;
	}

	return result;
}

sun::vec2 ParseUV(const FbxMesh* mesh, int controlPointIndex, int inTextureUVIndex)
{
	if (mesh->GetElementUVCount() < 1)
		return sun::vec2();

	sun::vec2 result;

	const FbxGeometryElementUV* vertexUV = mesh->GetElementUV(0);

	switch (vertexUV->GetMappingMode())
	{
	case FbxGeometryElement::eByControlPoint:
		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		{
			result.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(controlPointIndex).mData[0]);
			result.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(controlPointIndex).mData[1]);
		}
		break;

		case FbxGeometryElement::eIndexToDirect:
		{
			int index = vertexUV->GetIndexArray().GetAt(controlPointIndex);
			result.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[0]);
			result.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(index).mData[1]);
		}
		break;
		default:
			return sun::vec2();
		}
		break;

	case FbxGeometryElement::eByPolygonVertex:
		switch (vertexUV->GetReferenceMode())
		{
		case FbxGeometryElement::eDirect:
		case FbxGeometryElement::eIndexToDirect:
		{
			result.x = static_cast<float>(vertexUV->GetDirectArray().GetAt(inTextureUVIndex).mData[0]);
			result.y = static_cast<float>(vertexUV->GetDirectArray().GetAt(inTextureUVIndex).mData[1]);
		}
		break;
		default:
			return sun::vec2();
		}
		break;
	}

	return result;
}

void InsertVertex(const sun::vec3& position, const sun::vec3& normal, const sun::vec2& uv, const sun::vec3& binormal, const sun::vec3& tangent)
{
	sun::Vertex vertex = { position, normal, uv, binormal, tangent };

	auto lookup = s_indexMapping.find(vertex);

	if (lookup != s_indexMapping.end())
	{
		s_indices.push_back(lookup->second);
	}
	else
	{
		uint index = s_vertices.size();
		s_indexMapping[vertex] = index;
		s_indices.push_back(index);
		s_vertices.push_back(vertex);
	}
}