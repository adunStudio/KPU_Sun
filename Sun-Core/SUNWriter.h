#pragma once

#include <iostream>
#include <cstdlib>
#include <vector>

#include <fbxsdk.h>

#include "SunUtilities.h"

using namespace std;

namespace sun
{
	struct SUNFormat
	{
		char* header = "SUNH";
		byte nameLength;
		char* name;
		byte animationLength;
		uint vertexBufferSize;
		byte* vertexData;
		uint indexBufferSize;
		byte* indexData;
		char* footer = "SUNF";
	};

	class SUNWriter
	{
	private:
		String m_name;
		
		const vector<VertexWithBlending>& m_vertexWithBlending;
		const vector<uint  >& m_indexBuffer;
		const Skeleton      & m_skeleton;

		vector<vector<Vertex>> m_vertexBuffer;

		SUNFormat m_format;
	
		byte m_animationLength;

	private:
		void WriteBytes(FILE* file, const byte* data, uint size);

	public:
		SUNWriter(const String& name, const vector<VertexWithBlending>& vertexWithBlending, const vector<uint>& indices, const Skeleton skeleton, const size_t animationLength);
		~SUNWriter() {};

		void Write(const String& file);
	};
}