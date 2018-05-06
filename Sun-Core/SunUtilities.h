#pragma once

#include <string>
#include <unordered_map>

#include <fbxsdk.h>

#include "maths/maths.h"

typedef unsigned char byte;
typedef unsigned int uint;
typedef std::string String;

#define BIT(x) (1 << x)

using namespace sunny;
using namespace maths;

namespace sun
{
	struct KeyFrame
	{
		FbxLongLong frameNum;

		FbxAMatrix globalTransform;

		KeyFrame* next;

		KeyFrame() : next(nullptr) {};
	};

	struct Joint
	{
		String name;

		int parentIndex;

		FbxAMatrix globalBindPositionInverse;

		KeyFrame* animation;

		FbxNode* node;

		Joint() : animation(nullptr), node(nullptr), parentIndex(-1)
		{
			globalBindPositionInverse.SetIdentity();
		}

		~Joint()
		{
			while (animation)
			{
				KeyFrame* temp = animation->next;
				delete animation;
				animation = temp;
			}
		}
	};

	typedef std::vector<Joint> Skeleton;


	struct BlendingIndexWeightPair
	{
		uint blendingIndex;
		float blendingWeight;

		BlendingIndexWeightPair() : blendingIndex(0), blendingWeight(0.f)
		{}
	};

	struct Position
	{
		vec3 pos;
		std::vector<BlendingIndexWeightPair> blendingInfo;

		Position()
		{
			blendingInfo.reserve(8);
		}
	};

	struct VertexWithBlending
	{
		Position position;
		vec3 normal;
		vec2 uv;
		vec3 binormal;
		vec3 tangent;
		int id;

		bool operator==(const VertexWithBlending& other) const
		{
			return position.pos == other.position.pos && normal == other.normal && uv == other.uv && binormal == other.binormal && tangent == other.tangent;
		}
	};

	struct Vertex
	{
		vec3 position;
		vec3 normal;
		vec2 uv;
		vec3 binormal;
		vec3 tangent;
	};
}

template<>
struct std::hash<sun::VertexWithBlending>
{
	const size_t operator()(const sun::VertexWithBlending& key) const
	{
		return key.position.pos.GetHash() ^ key.normal.GetHash() ^ key.uv.GetHash() ^ key.binormal.GetHash() ^ key.tangent.GetHash() ^ key.id;
	}
};