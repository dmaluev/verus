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
			// Height:
			void SmoothHeight(short stepSize);
			void       SetHeightAt(const float xz[2], int amount, int radius);
			void   SetHeightFlatAt(const float xz[2], int amount, int radius, bool useNormal = false, PcVector3 pNormal = nullptr);
			void SetHeightSmoothAt(const float xz[2], int amount, int radius);

			// Normals:
			void UpdateNormalsForArea(int iMin, int iMax, int jMin, int jMax);

			// Splat:
			void SplatTileAt(const int ij[2], int channel, int amount);
			void SplatTileAtEx(const int ij[2], int layer, float maskValue, bool forceAll = false);
			void SwapSplatLayersAt(const int ij[2], int a, int b);
			bool IsSplatChannelBlankAt(const int ij[2], int channel) const;
			void SplatAt(const float xz[2], int layer, float amount, int radius,
				TerrainSplatMode mode = TerrainSplatMode::solid, const float* pMask = nullptr, bool updateTexture = true);
			void SplatFromFile(CSZ url, int layer);

			// Main layer:
			void UpdateMainLayerAt(const int ij[2]);
		};
		VERUS_TYPEDEFS(EditorTerrain);
	}
}
