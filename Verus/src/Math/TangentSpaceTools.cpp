// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Math;

void TangentSpaceTools::RecalculateNormals(
	const Vector<UINT16>& vIndices,
	const Vector<glm::vec3>& vPositions,
	Vector<glm::vec3>& vNormals,
	float areaBasedNormals)
{
	const int vertCount = Utils::Cast32(vPositions.size());
	const int faceCount = Utils::Cast32(vIndices.size() / 3);
	vNormals.resize(vertCount);

	Vector<glm::vec3> vFaceNormals;
	vFaceNormals.resize(faceCount);

	Vector<float> vFaceAreas;
	vFaceAreas.resize(faceCount);

	VERUS_P_FOR(i, faceCount)
	{
		const int a = vIndices[i * 3 + 0];
		const int b = vIndices[i * 3 + 1];
		const int c = vIndices[i * 3 + 2];

		float area = TriangleArea(vPositions[a], vPositions[b], vPositions[c]);
		area = glm::mix(1.f, area, areaBasedNormals);
		vFaceAreas[i] = area;

		const glm::vec3 v0(vPositions[a]);
		const glm::vec3 v1(vPositions[b]);
		const glm::vec3 v2(vPositions[c]);
		const glm::vec3 n = glm::triangleNormal(v0, v1, v2);
		vFaceNormals[i] = glm::normalize(n) * vFaceAreas[i];
	});

	VERUS_P_FOR(i, vertCount)
	{
		const int faceCount = Utils::Cast32(vIndices.size() / 3);
		const float threshold = 0.001f * 0.001f; // 1 mm.
		VERUS_FOR(j, faceCount)
		{
			const int a = vIndices[j * 3 + 0];
			const int b = vIndices[j * 3 + 1];
			const int c = vIndices[j * 3 + 2];

			if (a == i ||
				b == i ||
				c == i)
			{
				vNormals[i] += vFaceNormals[j];
			}
			else
			{
				const glm::vec3 prevNormal = vNormals[i];
				const float d0 = glm::distance2(vPositions[i], vPositions[a]);
				const float d1 = glm::distance2(vPositions[i], vPositions[b]);
				const float d2 = glm::distance2(vPositions[i], vPositions[c]);

				if (d0 < threshold || d1 < threshold || d2 < threshold)
				{
					vNormals[i] += vFaceNormals[j];
					if (glm::length2(vNormals[i]) < 0.01f * 0.01f) // Coplanar surface fix (1% of unit length):
						vNormals[i] = prevNormal;
				}
			}
		}
		vNormals[i] = glm::normalize(vNormals[i]);
	});
}

void TangentSpaceTools::RecalculateTangentSpace(
	const Vector<UINT16>& vIndices,
	const Vector<glm::vec3>& vPositions,
	const Vector<glm::vec3>& vNormals,
	const Vector<glm::vec2>& vTexCoords,
	Vector<glm::vec3>& vTan,
	Vector<glm::vec3>& vBin,
	bool useMikkTSpace)
{
	if (useMikkTSpace)
	{
		struct UserData
		{
			int _vertCount;
			int _faceCount;
			const UINT16* _pIndices;
			const glm::vec3* _pVerts;
			const glm::vec3* _pNormals;
			const glm::vec2* _pTexCoords;
			glm::vec3* _pTan;
			glm::vec3* _pBin;
		};
		VERUS_TYPEDEFS(UserData);

		UserData userData = {};
		userData._vertCount = Utils::Cast32(vPositions.size());
		userData._faceCount = Utils::Cast32(vIndices.size() / 3);
		userData._pIndices = vIndices.data();
		userData._pVerts = vPositions.data();
		userData._pNormals = vNormals.data();
		userData._pTexCoords = vTexCoords.data();
		vTan.resize(userData._vertCount);
		vBin.resize(userData._vertCount);
		userData._pTan = vTan.data();
		userData._pBin = vBin.data();

		auto GetNumFaces = [](const SMikkTSpaceContext* pContext) -> int
		{
			return static_cast<PcUserData>(pContext->m_pUserData)->_faceCount;
		};
		auto GetNumVerticesOfFace = [](const SMikkTSpaceContext* pContext, const int iFace) -> int
		{
			return 3;
		};
		auto GetPosition = [](const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			PcUserData p = static_cast<PcUserData>(pContext->m_pUserData);
			const int index = p->_pIndices[iFace * 3 + iVert];
			memcpy(fvPosOut, glm::value_ptr(p->_pVerts[index]), 3 * sizeof(float));
		};
		auto GetNormal = [](const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert)
		{
			PcUserData p = static_cast<PcUserData>(pContext->m_pUserData);
			const int index = p->_pIndices[iFace * 3 + iVert];
			memcpy(fvNormOut, glm::value_ptr(p->_pNormals[index]), 3 * sizeof(float));
		};
		auto GetTexCoord = [](const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert)
		{
			PcUserData p = static_cast<PcUserData>(pContext->m_pUserData);
			const int index = p->_pIndices[iFace * 3 + iVert];
			memcpy(fvTexcOut, glm::value_ptr(p->_pTexCoords[index]), 2 * sizeof(float));
		};
		auto SetTSpaceBasic = [](const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
		{
			PcUserData p = static_cast<PcUserData>(pContext->m_pUserData);
			const int index = p->_pIndices[iFace * 3 + iVert];
			memcpy(glm::value_ptr(p->_pTan[index]), fvTangent, 3 * sizeof(float));
			p->_pBin[index] = fSign * glm::cross(p->_pNormals[index], p->_pTan[index]);
		};

		SMikkTSpaceInterface inter = {};
		inter.m_getNumFaces = GetNumFaces;
		inter.m_getNumVerticesOfFace = GetNumVerticesOfFace;
		inter.m_getPosition = GetPosition;
		inter.m_getNormal = GetNormal;
		inter.m_getTexCoord = GetTexCoord;
		inter.m_setTSpaceBasic = SetTSpaceBasic;
		SMikkTSpaceContext context = {};
		context.m_pInterface = &inter;
		context.m_pUserData = &userData;
		genTangSpaceDefault(&context);
	}
	else
	{
		const int vertCount = Utils::Cast32(vPositions.size());
		const int faceCount = Utils::Cast32(vIndices.size() / 3);
		vTan.resize(vertCount);
		vBin.resize(vertCount);
		std::mutex m;
		const float degree = cos(glm::radians(1.f));

		VERUS_P_FOR(i, faceCount)
		{
			const int a = vIndices[i * 3 + 0];
			const int b = vIndices[i * 3 + 1];
			const int c = vIndices[i * 3 + 2];

			const glm::vec3 p1 = vPositions[a];
			const glm::vec3 p2 = vPositions[b];
			const glm::vec3 p3 = vPositions[c];
			const glm::vec2 t1 = vTexCoords[a];
			const glm::vec2 t2 = vTexCoords[b];
			const glm::vec2 t3 = vTexCoords[c];

			const glm::vec3 e1 = p2 - p1;
			const glm::vec3 e2 = p3 - p1;
			const glm::vec2 et1 = t2 - t1;
			const glm::vec2 et2 = t3 - t1;

			float tmp = 0;
			const float det = et1.x * et2.y - et1.y * et2.x;
			if (abs(det) < 1e-6f)
				tmp = 1;
			else
				tmp = 1 / det;

			const glm::vec3 tan = glm::normalize((e1 * et2.y - e2 * et1.y) * tmp);
			const glm::vec3 bin = glm::normalize((e2 * et1.x - e1 * et2.x) * tmp);

			{
				std::lock_guard<std::mutex> lock(m);
				vTan[a] += tan;
				vTan[b] += tan;
				vTan[c] += tan;
				vBin[a] += bin;
				vBin[b] += bin;
				vBin[c] += bin;
			}
		});

		VERUS_P_FOR(i, vertCount)
		{
			vTan[i] = glm::normalize(vTan[i]);
			vBin[i] = glm::normalize(vBin[i]);

			const glm::vec3 tmpT = vTan[i];
			const glm::vec3 tmpB = vBin[i];
			const glm::vec3 tmpN = vNormals[i];
			const glm::vec3 newT = glm::normalize(tmpT - (glm::dot(tmpN, tmpT) * tmpN));
			const glm::vec3 newB = glm::normalize(tmpB - (glm::dot(tmpN, tmpB) * tmpN) - (glm::dot(newT, tmpB) * newT));

			vTan[i] = newT;
			vBin[i] = newB;

			if (IsNaN(vTan[i].x) || IsNaN(vTan[i].y) || IsNaN(vTan[i].z))
				vTan[i] = glm::vec3(1, 0, 0);
			if (IsNaN(vBin[i].x) || IsNaN(vBin[i].y) || IsNaN(vBin[i].z))
				vBin[i] = glm::vec3(0, 0, 1);

			if (glm::dot(vBin[i], vTan[i]) >= degree)
				vBin[i] = glm::cross(tmpN, vTan[i]);
		});
	}
}
