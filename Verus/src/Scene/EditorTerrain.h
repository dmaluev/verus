// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		enum TerrainSplatMode
		{
			solid,
			grad,
			rand
		};

		class EditorTerrain : public Terrain
		{
		public:
			void ConvertToBufferCoords(const float xz[2], int ij[2]) const;
			glm::int4 ComputeBrushRect(const int ij[2], int radius) const;

			// Height:
			void SmoothenHeight(short stepSize);
			void   ApplyBrushHeight(const float xz[2], int radius, int strength);
			void  ApplyBrushFlatten(const float xz[2], int radius, int strength, bool useNormal = false, PcVector3 pNormal = nullptr);
			void ApplyBrushSmoothen(const float xz[2], int radius, int strength);

			// Normals:
			void UpdateNormalsForArea(const glm::int4& rc);

			// Splat:
			void           SplatTileAt(const int ij[2], int channel, int strength);
			void         SplatTileAtEx(const int ij[2], int layer, float maskValue, bool extended = false);
			void     SwapSplatLayersAt(const int ij[2], int a, int b);
			bool IsSplatChannelBlankAt(const int ij[2], int channel) const;
			bool        CanSplatTileAt(const int ij[2], int layer) const;
			void ApplyBrushSplat(const float xz[2], int layer, int radius, float strength,
				TerrainSplatMode mode = TerrainSplatMode::solid, const float* pMask = nullptr, bool updateTexture = true);
			void SplatFromFile(CSZ url, int layer);
		};
		VERUS_TYPEDEFS(EditorTerrain);
	}
}
