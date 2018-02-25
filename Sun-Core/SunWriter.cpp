#include "SUNWriter.h"

namespace sun
{
	SUNWriter::SUNWriter(const String& name, const vector<VertexWithBlending>& vertexWithBlending, const vector<uint>& indices, const Skeleton skeleton, const size_t animationLength)
	: m_name(name), m_vertexWithBlending(vertexWithBlending), m_indexBuffer(indices), m_skeleton(skeleton), m_animationLength((byte)animationLength)
	{
		m_vertexBuffer.resize(animationLength);

		for (uint i = 0; i < m_animationLength; ++i)
		{
			for (uint j = 0; j < m_vertexWithBlending.size(); ++j)
			{
				vec3 position = m_vertexWithBlending[j].position.pos;
				
				float weight1 = m_vertexWithBlending[j].position.blendingInfo[0].blendingWeight;
				float weight2 = m_vertexWithBlending[j].position.blendingInfo[1].blendingWeight;
				float weight3 = m_vertexWithBlending[j].position.blendingInfo[2].blendingWeight;
				float weight4 = m_vertexWithBlending[j].position.blendingInfo[3].blendingWeight;
				
				uint index1 = m_vertexWithBlending[j].position.blendingInfo[0].blendingIndex;
				uint index2 = m_vertexWithBlending[j].position.blendingInfo[1].blendingIndex;
				uint index3 = m_vertexWithBlending[j].position.blendingInfo[2].blendingIndex;
				uint index4 = m_vertexWithBlending[j].position.blendingInfo[3].blendingIndex;
				
				KeyFrame* anim1 = m_skeleton[index1].animation;
				KeyFrame* anim2 = m_skeleton[index2].animation;
				KeyFrame* anim3 = m_skeleton[index3].animation;
				KeyFrame* anim4 = m_skeleton[index4].animation;
				

				for (int time = 0; time < i; ++time)
				{
					anim1 = anim1->next;
					anim2 = anim2->next;
					anim3 = anim3->next;
					anim4 = anim4->next;
				}
				
				FbxMatrix a1 = m_skeleton[index1].globalBindPositionInverse * anim1->globalTransform;
				FbxMatrix a2 = m_skeleton[index2].globalBindPositionInverse * anim2->globalTransform;
				FbxMatrix a3 = m_skeleton[index3].globalBindPositionInverse * anim3->globalTransform;
				FbxMatrix a4 = m_skeleton[index4].globalBindPositionInverse * anim4->globalTransform;

				mat4 m1;
				mat4 m2;
				mat4 m3;
				mat4 m4;

				for (int mi = 0; mi < 4; ++mi)
					for (int mj = 0; mj < 4; ++mj)
						m1.elements[mi * 4 + mj] = a1.Get(mi, mj);

				for (int mi = 0; mi < 4; ++mi)
					for (int mj = 0; mj < 4; ++mj)
						m2.elements[mi * 4 + mj] = a2.Get(mi, mj);

				for (int mi = 0; mi < 4; ++mi)
					for (int mj = 0; mj < 4; ++mj)
						m3.elements[mi * 4 + mj] = a3.Get(mi, mj);

				for (int mi = 0; mi < 4; ++mi)
					for (int mj = 0; mj < 4; ++mj)
						m4.elements[mi * 4 + mj] = a4.Get(mi, mj);

				if (weight1 > 0.0f)
					position = position + (vec3(weight1) * position.Multiply(m1));
				if (weight2 > 0.0f)
					position = position + (vec3(weight2) * position.Multiply(m2));
				if (weight3 > 0.0f)
					position = position + (vec3(weight3) * position.Multiply(m3));
				if (weight4 > 0.0f)
					position = position + (vec3(weight4) * position.Multiply(m4));
				

				Vertex v = { position, m_vertexWithBlending[j].normal, m_vertexWithBlending[j].uv, m_vertexWithBlending[j].binormal, m_vertexWithBlending[j].tangent };
				
				m_vertexBuffer[i].push_back(v);
			}
		}

		m_format.nameLength = name.length();
		m_format.name = &m_name[0];
		m_format.animationLength = (size_t)m_animationLength;
		m_format.vertexBufferSize = m_vertexWithBlending.size() * sizeof(Vertex);
		
		//m_format.vertexData = (byte*)&m_vertexBuffer[0];
		
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
		WriteBytes(f, &format.animationLength, 1);
		WriteBytes(f, (byte*)&format.vertexBufferSize, sizeof(uint));

		for (uint i = 0; i < m_animationLength; ++i)
		{
			WriteBytes(f, (byte*)&m_vertexBuffer[i][0], format.vertexBufferSize);
		}

		WriteBytes(f, (byte*)&format.indexBufferSize, sizeof(uint));
		WriteBytes(f, format.indexData, format.indexBufferSize);
		WriteBytes(f, (byte*)format.footer, 4);
		
		fclose(f);
	}
}