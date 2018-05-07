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

static int s_skinid;
static int s_id = 0;
static FbxSkin* s_skin = nullptr;
static vector<sun::VertexWithBlending>                  s_vertices;     // 정점
static vector<uint>                                     s_indices;      // 인덱스
static unordered_map<sun::VertexWithBlending, uint>    s_indexMapping;  // 정점+인덱스 맵핑


static vector<vector<FbxAMatrix>> forNotD;


static sun::Position* s_rawPositions;                    // 정점 위치, 애니메이션
static uint s_rawPositionCount = 0;

static String s_name, s_inputName, s_outputName;


static FbxAMatrix s_rootMatrix;                            // STR

static sun::Skeleton s_skeleton;                         // 뼈(std::vector<Joint> joints;)

static bool s_hasAnimation;

static FbxTime s_AnimationStart, s_AnimationEnd;           // 애니메이션 시작과 종료 시간
static size_t s_AnimationLength = 1;                  // 애니메이션 길이(종료 - 시작)
static FbxAnimStack* s_animStack;

void LoadJoint(FbxNode* node, int depth, int index, int parentIndex);
void LoadNode(FbxNode* node);


bool      ParseMesh(FbxMesh* mesh);
void      ParseControlPoints(const FbxMesh* mesh);
vec3 ParseNormal(const FbxMesh* mesh, int controlPointIndex, int vertexCount);
vec3 ParseBinormal(const FbxMesh* mesh, int controlPointIndex, int vertexCount);
vec3 ParseTangent(FbxMesh* mesh, int controlPointIndex, int vertexCount);
vec2 ParseUV(const FbxMesh* mesh, int controlPointIndex, int inTextureUVIndex);

void      ParseAnimation(FbxNode* node);

//void InsertVertex(const vec3& position, const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent);
void InsertVertex(const uint rawPositionIndex, const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent, int isSkin);

int main()
{
	cout << "FBX 파일명을 입력해주세요.: ";

	cin >> s_name;

	s_inputName = "Meshes/" + s_name + ".fbx";
	//s_outputName = "Meshes/" + s_name + ".fbx";
	s_outputName = "C:/Users/adunstudio/Desktop/Sunny/Sunny-Core/04_ASSET/SUN/" + s_name + ".sun";

	FbxManager*  manager = FbxManager::Create();
	FbxScene*      scene = FbxScene::Create(manager, "scene");
	FbxImporter* importer = FbxImporter::Create(manager, "");

	forNotD.resize(100);
	

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
		FbxAxisSystem sceneAxisSystem = scene->GetGlobalSettings().GetAxisSystem();
		FbxAxisSystem ourAxisSystem(FbxAxisSystem::eOpenGL);
		if (sceneAxisSystem != ourAxisSystem)
			ourAxisSystem.ConvertScene(scene);

		/*FbxAxisSystem::MayaYUp.ConvertScene(scene);*/

		// 씬 내에서 삼각형화 할 수 있는 모든 노드를 삼각형화 시킨다.
		FbxGeometryConverter geometryConverter(manager);
		geometryConverter.Triangulate(scene, true);

		s_animStack = scene->GetSrcObject<FbxAnimStack>();

		if (s_animStack)
		{
			FbxString animStackName = s_animStack->GetName();

			std::cout << "animStackName" << animStackName << endl;
			FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStackName);

			s_AnimationStart = takeInfo->mLocalTimeSpan.GetStart();
			s_AnimationEnd = takeInfo->mLocalTimeSpan.GetStop();

			s_AnimationLength = s_AnimationEnd.GetFrameCount(FbxTime::eFrames24) - s_AnimationStart.GetFrameCount(FbxTime::eFrames24) + 1;
		}

		LoadJoint(scene->GetRootNode(), -1, -1, -1);

		s_hasAnimation = s_skeleton.size() > 0 ? true : false;

		LoadNode(scene->GetRootNode());

		sun::SUNWriter writer(s_name, s_vertices, s_indices, s_skeleton, s_AnimationLength, forNotD);
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

		for (int i = 0; i < parentIndex; ++i)
			cout << "\t";

		std::cout << "joint.name: " << joint.name << std::endl;

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

		int materialCount = node->GetSrcObjectCount<FbxSurfaceMaterial>();
		if (materialCount > 0)
			std::cout << "materialCount: " << materialCount << std::endl;

		for (int index = 0; index < materialCount; ++index)
		{
			FbxSurfaceMaterial* material = (FbxSurfaceMaterial*)node->GetSrcObject<FbxSurfaceMaterial>(index);
			if (!material) continue;

			std::cout << "texture: " << material->GetName() << std::endl;
		}

		FbxMesh* mesh = node->GetMesh();

		ParseControlPoints(mesh);

		if (s_hasAnimation )
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


	int materialCount = mesh->GetSrcObjectCount<FbxSurfaceMaterial>();

	uint triangleCount = mesh->GetPolygonCount();

	uint vertexCount = 0;

	s_id++;

	std::cout << "s_id: " << s_id << std::endl;

	for (int triangle = 0; triangle < triangleCount; ++triangle)
	{
		vec3 tanget;
		vec3 binormal;
		vec2 uv;

		int id = 0;


		for (int a = 0; a < mesh->GetElementMaterialCount(); a++)
		{

			FbxGeometryElementMaterial* lMaterialElement = mesh->GetElementMaterial(a);
			FbxSurfaceMaterial* lMaterial = NULL;
			int lMatId = -1;
			lMaterial = mesh->GetNode()->GetMaterial(lMaterialElement->GetIndexArray().GetAt(triangle));
			id = lMaterialElement->GetIndexArray().GetAt(triangle);
		}


		for (uint i = 0; i < 3; ++i)
		{
			int controlPointIndex = mesh->GetPolygonVertex(triangle, i);
			
			vec3& position = s_rawPositions[controlPointIndex].pos;
			vec3    normal = ParseNormal(mesh, controlPointIndex, vertexCount);
			vec3  binormal = ParseBinormal(mesh, controlPointIndex, vertexCount);
			//vec3   tangent = ParseTangent(mesh, controlPointIndex, vertexCount);
			vec3   tangent = {1.0f *  id, 1.0f * id, 1.0f * id };// ParseTangent(mesh, controlPointIndex, vertexCount);
			vec2        uv = ParseUV(mesh, controlPointIndex, mesh->GetTextureUVIndex(triangle, i));
			
			
			FbxGeometryElementMaterial* vertexMaterial =  mesh->GetElementMaterial(0);
		

			uv.y = 1.0f - uv.y;
			//uv.x = 1.0f - uv.x;
	
			int isSkinMesh = s_rawPositions[controlPointIndex].isSkinMesh;
			
			InsertVertex(controlPointIndex, normal, uv, binormal, tangent, isSkinMesh);

			vertexCount++;
			
			
		}
	}

	std::cout << "--------------" << s_vertices.size() << "--------------" << std::endl;
	std::cout << "--------------" << s_indices.size() << "--------------" << std::endl;
	

	return true;
}

void ParseControlPoints(const FbxMesh* mesh)
{
	
	s_rawPositionCount = mesh->GetControlPointsCount();

	std::cout << "s_rawPositionCount: " << s_rawPositionCount << std::endl;

	s_rawPositions = new sun::Position[s_rawPositionCount];

	for (uint i = 0; i < s_rawPositionCount; ++i)
	{
		vec3 position;
		position.x = static_cast<float>(mesh->GetControlPointAt(i).mData[0]);
		position.y = static_cast<float>(mesh->GetControlPointAt(i).mData[1]);
		position.z = static_cast<float>(mesh->GetControlPointAt(i).mData[2]);

		s_rawPositions[i].pos = position;
		// rawPosition 문제
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

	//result.y *= -1;
	//result.z *=  -1;

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
	{
		std::cout << "not uvcount" << std::endl;
		return vec2();

	}

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

void InsertVertex(const uint rawPositionIndex, const vec3& normal, const vec2& uv, const vec3& binormal, const vec3& tangent, int isSkin)
{
	sun::VertexWithBlending vertex = { s_rawPositions[rawPositionIndex], normal, uv, binormal, tangent, s_id, isSkin };

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
	FbxGeometry* geo = node->GetGeometry();

	s_rootMatrix.SetIdentity();

	const FbxVector4 T = node->GetGeometricTranslation(FbxNode::eSourcePivot);
	const FbxVector4 R = node->GetGeometricRotation(FbxNode::eSourcePivot);
	const FbxVector4 S = node->GetGeometricScaling(FbxNode::eSourcePivot);

	s_rootMatrix = FbxAMatrix(T, R, S);

	
	uint deformerCount = geo->GetDeformerCount();

	if (deformerCount > 0)
	{
		// 보통 1개
		for (int deformerIndex = 0; deformerIndex < deformerCount; ++deformerIndex)
		{
			s_skin = reinterpret_cast<FbxSkin*>(geo->GetDeformer(deformerIndex, FbxDeformer::eSkin));

			if (!s_skin) continue;

			uint clusterCount = s_skin->GetClusterCount();

			// 클러스터 안의 link가 joint 역할

			for (uint clusterIndex = 0; clusterIndex < clusterCount; ++clusterIndex)
			{
				FbxCluster* cluster = s_skin->GetCluster(clusterIndex);

				FbxAMatrix transformMatrix;
				FbxAMatrix transformLinkMatrix;
				FbxAMatrix globalBindposeInverseMatrix;

				cluster->GetTransformMatrix(transformMatrix);
				cluster->GetTransformLinkMatrix(transformLinkMatrix);


				globalBindposeInverseMatrix = transformLinkMatrix.Inverse() * transformMatrix * s_rootMatrix;
				unsigned int jointIndex;

				String jointName = cluster->GetLink()->GetName();
				std::cout << jointName << std::endl;
				for (uint i = 0; i < s_skeleton.size(); ++i)
					if (s_skeleton[i].name == jointName)
						jointIndex = i;

				s_skeleton[jointIndex].globalBindPositionInverse = globalBindposeInverseMatrix;
				s_skeleton[jointIndex].node = cluster->GetLink();

				uint IndicesCount = cluster->GetControlPointIndicesCount();

				std::cout << "indicesCount: " << IndicesCount << std::endl;

				for (uint i = 0; i < IndicesCount; ++i)
				{
					sun::BlendingIndexWeightPair blendingIndexWeightPair;

					blendingIndexWeightPair.blendingIndex = jointIndex;
					blendingIndexWeightPair.blendingWeight = (float)(cluster->GetControlPointWeights()[i]);
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

					const FbxVector4 Tr = cluster->GetLink()->GetGeometricTranslation(FbxNode::eSourcePivot);
					const FbxVector4 Rr = cluster->GetLink()->GetGeometricRotation(FbxNode::eSourcePivot);
					const FbxVector4 Sr = cluster->GetLink()->GetGeometricScaling(FbxNode::eSourcePivot);

					//s_rootMatrix = ;

					FbxAMatrix currentTransformOffset = node->EvaluateGlobalTransform(time) *FbxAMatrix(Tr, Rr, Sr);
					FbxAMatrix globalTransform = /*currentTransformOffset.Inverse() */ cluster->GetLink()->EvaluateGlobalTransform(time);

					(*anim)->globalTransform = globalTransform;

					anim = &((*anim)->next);
				}
			}
		}

		// 8개이하 가중치 0처리
		sun::BlendingIndexWeightPair blendingIndexWeightPair;

		blendingIndexWeightPair.blendingIndex = 0;
		blendingIndexWeightPair.blendingWeight = 0;

		for (uint rawPositionIndex = 0; rawPositionIndex < s_rawPositionCount; ++rawPositionIndex)
		{
			s_rawPositions[rawPositionIndex].isSkinMesh = -1;
			for (uint i = s_rawPositions[rawPositionIndex].blendingInfo.size(); i <= 8; ++i)
			{
				s_rawPositions[rawPositionIndex].blendingInfo.push_back(blendingIndexWeightPair);
			}
		}
	}
	else
	{
		
		for (FbxLongLong i = s_AnimationStart.GetFrameCount(FbxTime::eFrames24); i <= s_AnimationEnd.GetFrameCount(FbxTime::eFrames24); ++i)
		{
			FbxTime time;
			time.SetFrame(i, FbxTime::eFrames24);

			FbxAMatrix currentTransformOffset = node->EvaluateGlobalTransform(time);
			FbxAMatrix result = currentTransformOffset * s_rootMatrix;
			//result = result.Inverse();

			forNotD[s_skinid].push_back(result);
		}

		for (uint rawPositionIndex = 0; rawPositionIndex < s_rawPositionCount; ++rawPositionIndex)
		{
			s_rawPositions[rawPositionIndex].isSkinMesh = s_skinid;
			
		}

		++s_skinid;
	}
	

	

}
