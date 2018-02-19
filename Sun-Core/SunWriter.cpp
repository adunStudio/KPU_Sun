#include "SUNWriter.h"

namespace sun
{
	SUNWriter::SUNWriter(const String& name, const vector<Vertex>& vertices, const vector<uint>& indices)
	: m_name(name), m_vertexBuffer(vertices), m_indexBuffer(indices)
	{
		m_format.nameLength = name.length();
		m_format.name = &m_name[0];
		m_format.vertexBufferSize = m_vertexBuffer.size() * sizeof(Vertex);
		m_format.vertexData = (byte*)&m_vertexBuffer[0];
		m_format.indexBufferSize = m_indexBuffer.size() * sizeof(uint);
		m_format.indexData = (byte*)&m_indexBuffer[0];
	}

	void SUNWriter::WriteBytes(FILE* file, const byte* data, uint size)
	{
		fwrite(data, 1, size, file);
	}

	void SUNWriter::Write(const String& file)
	{
		const SUNFormat& format = m_format;

		FILE* f = fopen(file.c_str(), "wb");

		WriteBytes(f, (byte*)format.header, 4);
		WriteBytes(f, &format.nameLength, 1);
		WriteBytes(f, (byte*)format.name, format.nameLength);
		WriteBytes(f, (byte*)&format.vertexBufferSize, sizeof(uint));
		WriteBytes(f, format.vertexData, format.vertexBufferSize);
		WriteBytes(f, (byte*)&format.indexBufferSize, sizeof(uint));
		WriteBytes(f, format.indexData, format.indexBufferSize);
		WriteBytes(f, (byte*)format.footer, 4);
		
		fclose(f);
	}
}