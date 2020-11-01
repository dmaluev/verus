// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Math
	{
		class NormalComputer
		{
		public:
			static void ComputeNormals(
				const Vector<UINT16>& vIndices,
				const Vector<glm::vec3>& vPositions,
				Vector<glm::vec3>& vNormals,
				float areaBasedNormals);

			static void ComputeTangentSpace(
				const Vector<UINT16>& vIndices,
				const Vector<glm::vec3>& vPositions,
				const Vector<glm::vec3>& vNormals,
				const Vector<glm::vec2>& vTexCoords,
				Vector<glm::vec3>& vTan,
				Vector<glm::vec3>& vBin);
		};
		VERUS_TYPEDEFS(NormalComputer);
	}
}
