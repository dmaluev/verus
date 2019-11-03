#include "verus.h"

using namespace verus;
using namespace verus::Math;

void NormalComputer::ComputeNormals(
	const Vector<UINT16>& vIndices,
	const Vector<glm::vec3>& vPositions,
	Vector<glm::vec3>& vNormals,
	float areaBasedNormals)
{
	const int numVerts = Utils::Cast32(vPositions.size());
	const int numFaces = Utils::Cast32(vIndices.size() / 3);
	vNormals.resize(numVerts);

	Vector<glm::vec3> vFaceNormals;
	vFaceNormals.resize(numFaces);

	Vector<float> vFaceAreas;
	vFaceAreas.resize(numFaces);

	VERUS_P_FOR(i, numFaces)
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

	VERUS_P_FOR(i, numVerts)
	{
		const int numFaces = Utils::Cast32(vIndices.size() / 3);
		const float threshold = 0.001f * 0.001f; // 1 mm.
		VERUS_FOR(j, numFaces)
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

void NormalComputer::ComputeTangentSpace(
	const Vector<UINT16>& vIndices,
	const Vector<glm::vec3>& vPositions,
	const Vector<glm::vec3>& vNormals,
	const Vector<glm::vec2>& vTexCoords,
	Vector<glm::vec3>& vTan,
	Vector<glm::vec3>& vBin)
{
	const int numVerts = Utils::Cast32(vPositions.size());
	const int numFaces = Utils::Cast32(vIndices.size() / 3);
	vTan.resize(numVerts);
	vBin.resize(numVerts);
	std::mutex m;
	const float degree = cos(glm::radians(1.f));

	VERUS_P_FOR(i, numFaces)
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

	VERUS_P_FOR(i, numVerts)
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
