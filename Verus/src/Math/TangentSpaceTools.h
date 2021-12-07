// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Math
	{
		class TangentSpaceTools
		{
		public:
			static void RecalculateNormals(
				const Vector<UINT16>& vIndices,
				const Vector<glm::vec3>& vPositions,
				Vector<glm::vec3>& vNormals,
				float areaBasedNormals);

			static void RecalculateTangentSpace(
				const Vector<UINT16>& vIndices,
				const Vector<glm::vec3>& vPositions,
				const Vector<glm::vec3>& vNormals,
				const Vector<glm::vec2>& vTexCoords,
				Vector<glm::vec3>& vTan,
				Vector<glm::vec3>& vBin,
				bool useMikkTSpace = true);
		};
		VERUS_TYPEDEFS(TangentSpaceTools);
	}
}
