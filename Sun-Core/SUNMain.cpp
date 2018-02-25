#include <iostream>
#include <string>
#include <vector>

#include <fbxsdk.h>

#include "SunUtilities.h"
#include "maths/maths.h"
#include "SUNWriter.h"

using namespace std;
using namespace sunny;
using namespace maths;

static vector<sun::VertexWithBlending>                  s_vertices;     // 정점
static vector<uint>                                     s_indices;      // 인덱스
static unordered_map<sun::VertexWithBlending, uint>    s_indexMapping;  // 정점+인덱스 맵핑

static sun::Position* s_rawPositions;                    // 정점 위치, 애니메이션
static uint s_rawPositionCount;

static String s_name, s_inputName, s_outputName;


static FbxAMatrix s_rootMatrix;                            // STR

static sun::Skeleton s_skeleton;                         // 뼈(std::vector<Joint> joints;)

static bool s_hasAnimation;

static FbxTime s_AnimationStart, s_AnimationEnd;           // 애니메이션 시작과 종료 시간
static size_t s_AnimationLength = 1;                  // 애니메이션 길이(종료 - 시작)

void LoadJoint(FbxNode* node, int depth, int index, int parentIndex);
void LoadNode(FbxNode* node);


bool      ParseMesh         (      FbxMesh* mesh);
void      ParseControlPoints(const FbxMesh* mesh);
vec3 ParseNormal       (const FbxMesh* mesh, int controlPointIndex, int vertexCount);
vec3 ParseBinormal     (const FbxMesh* mesh, int controlPointIndex, int vertexCount);
vec3 ParseTangent      (      FbxMesh* mesh, int controlPointIndex, int vertexCount);
vec2 ParseUV           (const FbxMesh* mesh, int controlPointIndex, int inTextureUVIndex);

void      ParseAnimation    (      FbxNode* node);

//void InsertVertex(const vec3& position, const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent);
void InsertVertex(const uint rawPositionIndex , const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent);

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

		FbxAnimStack* animStack = scene->GetSrcObject<FbxAnimStack>(0);

		if (animStack)
		{
			FbxString animStackName = animStack->GetName();
			FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStackName);

			s_AnimationStart = takeInfo->mLocalTimeSpan.GetStart();
			s_AnimationEnd   = takeInfo->mLocalTimeSpan.GetStop();
			s_AnimationLength = s_AnimationEnd.GetFrameCount(FbxTime::eFrames24) - s_AnimationStart.GetFrameCount(FbxTime::eFrames24) + 1;
		}

		LoadJoint(scene->GetRootNode(), -1, -1, -1);

		s_hasAnimation = s_skeleton.size() > 0 ? true : false;

		LoadNode (scene->GetRootNode());

		sun::SUNWriter writer(s_name, s_vertices, s_indices, s_skeleton, s_AnimationLength);
		writer.Write(s_outputName);
	}

	cout << "■■■■■■■■■■■■■■■■■■■" << endl;
	cout << "Complete Convert " << s_inputName << " To " << s_outputName << endl;
	cout << "■■■■■■■■■■■■■■■■■■■";

	getchar();
	getchar();

	return 1;
}

void LoadJoint(FbxNode* node, int depth, int index, int parentIndex)
{

	FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();

	if (nodeAttribute && nodeAttribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		sun::Joint joint;
		joint.parentIndex = parentIndex;
		joint.name = node->GetName();

		s_skeleton.push_back(joint);
	}

	const uint childCount = node->GetChildCount();

	// 재귀
	for (uint i = 0; i < childCount; ++i)
		LoadJoint(node->GetChild(i), depth + 1, s_skeleton.size(), index);
}


void LoadNode(FbxNode* node)
{
	FbxNodeAttribute* nodeAttribute = node->GetNodeAttribute();

	if (nodeAttribute && nodeAttribute->GetAttributeType() == FbxNodeAttribute::eMesh)
	{
		std::cout << "Mesh" << std::endl;
		FbxMesh* mesh = node->GetMesh();

		ParseControlPoints(mesh);
		
		if (s_hasAnimation)
			ParseAnimation(node);

		ParseMesh(mesh);
	}

	const uint childCount = node->GetChildCount();

	// 재귀
	for (uint i = 0; i < childCount; ++i)
		LoadNode(node->GetChild(i));
}

bool ParseMesh(FbxMesh* mesh)
{
	if (!mesh->GetNode())
		return false;

	uint triangleCount = mesh->GetPolygonCount();

	uint vertexCount = 0;

	for (int triangle = 0; triangle < triangleCount; ++triangle)
	{
		vec3 tanget;
		vec3 binormal;
		vec2 uv;


		for (uint i = 0; i < 3; ++i)
		{
			int controlPointIndex = mesh->GetPolygonVertex(triangle, i);

			vec3& position = s_rawPositions[controlPointIndex].pos;
			vec3    normal = ParseNormal  (mesh, controlPointIndex, vertexCount);
			vec3  binormal = ParseBinormal(mesh, controlPointIndex, vertexCount);
			vec3   tangent = ParseTangent (mesh, controlPointIndex, vertexCount);
			vec2        uv = ParseUV      (mesh, controlPointIndex, mesh->GetTextureUVIndex(triangle, i));

			uv.y = 1.0f - uv.y;
			//position.z = position.z * -1.0f;
			normal.z = normal.z * -1.0f;

			InsertVertex(controlPointIndex, normal, uv, binormal, tangent);

			vertexCount++;
		}
	}

	

	return true;
}

void ParseControlPoints(const FbxMesh* mesh)
{
	s_rawPositionCount = mesh->GetControlPointsCount();

	s_rawPositions = new sun::Position[s_rawPositionCount];

	for (uint i = 0; i < s_rawPositionCount; ++i)
	{
		vec3 position;
		position.x = static_cast<float>(mesh->GetControlPointAt(i).mData[0]);
		position.y = static_cast<float>(mesh->GetControlPointAt(i).mData[1]);
		position.z = static_cast<float>(mesh->GetControlPointAt(i).mData[2]);

		s_rawPositions[i].pos = position;
	}
}

vec3 ParseNormal(const FbxMesh* mesh, int controlPointIndex, int vertexCount)
{
	if (mesh->GetElementNormalCount() < 1)
		return vec3();

	vec3 result;

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
			return vec3();
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
			return vec3();
		}
		break;
	}

	return result;
}

vec3 ParseBinormal(const FbxMesh* mesh, int controlPointIndex, int vertexCount)
{
	if (mesh->GetElementBinormalCount() < 1)
		return vec3();

	vec3 result;

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
			return vec3();
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
			return vec3();
		}
		break;
	}

	return result;
}

vec3 ParseTangent(FbxMesh* mesh, int controlPointIndex, int vertexCount)
{
	if (mesh->GetElementTangentCount() < 1)
		return vec3();

	vec3 result;

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
			return vec3();
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
			return vec3();
		}
		break;
	}

	return result;
}

vec2 ParseUV(const FbxMesh* mesh, int controlPointIndex, int inTextureUVIndex)
{
	if (mesh->GetElementUVCount() < 1)
		return vec2();

	vec2 result;

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
			return vec2();
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
			return vec2();
		}
		break;
	}

	return result;
}

/*void InsertVertex(const vec3& position, const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent)
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
}*/

void InsertVertex(const uint rawPositionIndex, const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent)
{
	sun::VertexWithBlending vertex = { s_rawPositions[rawPositionIndex], normal, uv, binormal, tangent };

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

void ParseAnimation(FbxNode* node)
{
	FbxMesh* mesh = node->GetMesh();

	const FbxVector4 S = node->GetGeometricScaling    (FbxNode::eSourcePivot);
	const FbxVector4 T = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 R = node->GetGeometricRotation   (FbxNode::eSourcePivot);

	s_rootMatrix = FbxAMatrix(T, R, S);
	//s_rootMatrix = FbxAMatrix(S, T, R);

	uint deformerCount = mesh->GetDeformerCount();

	// 보통 1개
	for (int deformerIndex = 0; deformerIndex < deformerCount; ++deformerIndex)
	{
		FbxSkin* skin = reinterpret_cast<FbxSkin*>(mesh->GetDeformer(deformerIndex, FbxDeformer::eSkin));

		if (!skin) continue;

		uint clusterCount = skin->GetClusterCount();

		// 클러스터 안의 link가 joint 역할

		for (uint clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex)
		{
			FbxCluster* cluster = skin->GetCluster(clusterIndex);

			FbxAMatrix transformMatrix;
			FbxAMatrix transformLinkMatrix;
			FbxAMatrix globalBindposeInverseMatrix;

			cluster->GetTransformMatrix(transformMatrix);
			cluster->GetTransformLinkMatrix(transformLinkMatrix);
			globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * s_rootMatrix;
		
			unsigned int jointIndex;

			String jointName = cluster->GetLink()->GetName();

			for (uint i = 0; i < s_skeleton.size(); ++i)
				if (s_skeleton[i].name == jointName)
					jointIndex = i;

			s_skeleton[jointIndex].globalBindPositionInverse = globalBindposeInverseMatrix;
			s_skeleton[jointIndex].node = cluster->GetLink();

			uint IndicesCount = cluster->GetControlPointIndicesCount();

			for (uint i = 0; i < IndicesCount; ++i)
			{
				sun::BlendingIndexWeightPair blendingIndexWeightPair;

				blendingIndexWeightPair.blendingIndex  = jointIndex;
				blendingIndexWeightPair.blendingWeight = cluster->GetControlPointWeights()[i];

				s_rawPositions[cluster->GetControlPointIndices()[i]].blendingInfo.push_back(blendingIndexWeightPair);
			}

			// ㄴ조인트 설정 및 각 포지션별 조인트 인덱스 설정 완료

			// 애니메이션 시작
			sun::KeyFrame** anim = &s_skeleton[jointIndex].animation;

			for (FbxLongLong i = s_AnimationStart.GetFrameCount(FbxTime::eFrames24); i <= s_AnimationEnd.GetFrameCount(FbxTime::eFrames24); ++i)
			{
				FbxTime time;
				time.SetFrame(i, FbxTime::eFrames24);
				*anim = new sun::KeyFrame();
				(*anim)->frameNum = i;
				FbxAMatrix currentTransformOffset = node->EvaluateGlobalTransform(time) * s_rootMatrix;
				FbxAMatrix globalTransform = currentTransformOffset.Inverse() * cluster->GetLink()->EvaluateGlobalTransform(time);
				
				(*anim)->globalTransform = currentTransformOffset.Inverse() * cluster->GetLink()->EvaluateGlobalTransform(time);
				
				anim = &((*anim)->next);
			}
		}
	}

	// 4개이하 가중치 0처리
	sun::BlendingIndexWeightPair blendingIndexWeightPair;

	blendingIndexWeightPair.blendingIndex = 0;
	blendingIndexWeightPair.blendingWeight = 0;

	for (uint rawPositionIndex = 0; rawPositionIndex < s_rawPositionCount; ++rawPositionIndex)
	{
		for (uint i = s_rawPositions[rawPositionIndex].blendingInfo.size(); i <= 4; ++i)
		{
			if(rawPositionIndex == 0)
				std::cout << rawPositionIndex << " : " << i << endl;
			s_rawPositions[rawPositionIndex].blendingInfo.push_back(blendingIndexWeightPair);
		}
	}

}
