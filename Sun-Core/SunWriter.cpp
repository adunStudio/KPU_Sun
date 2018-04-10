#include "SUNWriter.h"

namespace sun
{
	SUNWriter::SUNWriter(const String& name, const vector<VertexWithBlending>& vertexWithBlending, const vector<uint>& indices, const Skeleton skeleton, const size_t animationLength)
	: m_name(name), m_vertexWithBlending(vertexWithBlending), m_indexBuffer(indices), m_skeleton(skeleton), m_animationLength((byte)animationLength)
	{
		m_vertexBuffer.resize(animationLength);

		std::cout << "animation: " << animationLength << std::endl;
		
		std::cout << m_vertexWithBlending.size() << std::endl;

		if (animationLength > 1)
		{
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



					FbxAMatrix a1 = anim1->globalTransform *  m_skeleton[index1].globalBindPositionInverse;
					FbxAMatrix a2 = anim2->globalTransform *  m_skeleton[index2].globalBindPositionInverse;
					FbxAMatrix a3 = anim3->globalTransform *  m_skeleton[index3].globalBindPositionInverse;
					FbxAMatrix a4 = anim4->globalTransform *  m_skeleton[index4].globalBindPositionInverse;

					FbxVector4 pos = { position.x, position.y, position.z, 1 };
					FbxVector4 tmp = { position.x, position.y, position.z, 1 };


					a1 = a1.Transpose();
					a2 = a2.Transpose();
					a3 = a3.Transpose();
					a4 = a4.Transpose();

					float a11 = a1.Get(0, 0) * tmp[0] + a1.Get(0, 1) * tmp[1] + a1.Get(0, 2) * tmp[2] + a1.Get(0, 3);// *tmp[3];
					float b11 = a1.Get(1, 0) * tmp[0] + a1.Get(1, 1) * tmp[1] + a1.Get(1, 2) * tmp[2] + a1.Get(1, 3);// * tmp[3];
					float c11 = a1.Get(2, 0) * tmp[0] + a1.Get(2, 1) * tmp[1] + a1.Get(2, 2) * tmp[2] + a1.Get(2, 3);// * tmp[3];


					float a22 = a2.Get(0, 0) * tmp[0] + a2.Get(0, 1) * tmp[1] + a2.Get(0, 2) * tmp[2] + a2.Get(0, 3);// * tmp[3];
					float b22 = a2.Get(1, 0) * tmp[0] + a2.Get(1, 1) * tmp[1] + a2.Get(1, 2) * tmp[2] + a2.Get(1, 3);// * tmp[3];
					float c22 = a2.Get(2, 0) * tmp[0] + a2.Get(2, 1) * tmp[1] + a2.Get(2, 2) * tmp[2] + a2.Get(2, 3);// * tmp[3];

					float a33 = a3.Get(0, 0) * tmp[0] + a3.Get(0, 1) * tmp[1] + a3.Get(0, 2) * tmp[2] + a3.Get(0, 3);// * tmp[3];
					float b33 = a3.Get(1, 0) * tmp[0] + a3.Get(1, 1) * tmp[1] + a3.Get(1, 2) * tmp[2] + a3.Get(1, 3);// * tmp[3];
					float c33 = a3.Get(2, 0) * tmp[0] + a3.Get(2, 1) * tmp[1] + a3.Get(2, 2) * tmp[2] + a3.Get(2, 3);// * tmp[3];

					float a44 = a4.Get(0, 0) * tmp[0] + a4.Get(0, 1) * tmp[1] + a4.Get(0, 2) * tmp[2] + a4.Get(0, 3);// * tmp[3];
					float b44 = a4.Get(1, 0) * tmp[0] + a4.Get(1, 1) * tmp[1] + a4.Get(1, 2) * tmp[2] + a4.Get(1, 3);// * tmp[3];
					float c44 = a4.Get(2, 0) * tmp[0] + a4.Get(2, 1) * tmp[1] + a4.Get(2, 2) * tmp[2] + a4.Get(2, 3);// * tmp[3];


					if (weight1 > 0.0f)
						pos = FbxVector4(a11, b11, c11, 1) * weight1;
					if (weight2 > 0.0f)
						pos = FbxVector4(a22, b22, c22, 1) * weight2;
					if (weight3 > 0.0f)
						pos = FbxVector4(a33, b33, c33, 1) * weight3;
					if (weight4 > 0.0f)
						pos = FbxVector4(a44, b44, c44, 1) * weight4;


					vec3 result = { (float)pos[0], (float)pos[1], (float)pos[2] };
					Vertex v = { result, m_vertexWithBlending[j].normal, m_vertexWithBlending[j].uv, m_vertexWithBlending[j].binormal, m_vertexWithBlending[j].tangent };

					m_vertexBuffer[i].push_back(v);
				}
			}
		}
		else
		{
			
			std::cout << 333 << std::endl;
			for (uint j = 0; j < m_vertexWithBlending.size(); ++j)
			{
				vec3 position = m_vertexWithBlending[j].position.pos;
				FbxVector4 pos = { position.x, position.y, position.z, 1 };


				vec3 result = { (float)pos[0], (float)pos[1], (float)pos[2] };
				Vertex v = { result, m_vertexWithBlending[j].normal, m_vertexWithBlending[j].uv, m_vertexWithBlending[j].binormal, m_vertexWithBlending[j].tangent };

				m_vertexBuffer[0].push_back(v);
		
			}
		}
				


		m_format.nameLength = name.length();
		m_format.name = &m_name[0];
		m_format.animationLength = (size_t)m_animationLength ;
		m_format.vertexBufferSize = m_vertexWithBlending.size() * sizeof(Vertex);
				
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