#pragma once

#include <iostream>
#include <cstdlib>
#include <vector>

#include "SUNMaths.h"

using namespace std;

namespace sun
{
	struct SUNFormat
	{
		char* header = "SUNH";
		byte nameLength;
		char* name;
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
		
		const vector<Vertex>& m_vertexBuffer;
		const vector<uint  >& m_indexBuffer;

		SUNFormat m_format;
	
	private:
		void WriteBytes(FILE* file, const byte* data, uint size);

	public:
		SUNWriter(const String& name, const vector<Vertex>& vertices, const vector<uint>& indices);
		~SUNWriter() {};

		void Write(const String& file);
	};
}