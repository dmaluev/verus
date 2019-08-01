#pragma once

namespace verus
{
	namespace Scene
	{
		class TerrainLOD
		{
		public:
			Vector<UINT16> _vIB;
			int            _side = 0;
			int            _numVerts = 0;
			int            _numIndices = 0;
			int            _startIndex = 0;

			void Init(int sidePoly, int step);
			void InitGeo(short* pV, UINT16* pI, int baseVertex);
		};
		VERUS_TYPEDEFS(TerrainLOD);

		class TerrainPatch
		{
		public:
		};
		VERUS_TYPEDEFS(TerrainPatch);

		class Terrain : public Object
		{
		public:
#include "../Shaders/DS_Terrain.inc.hlsl"

			static const int s_maxLayers = 16;

			struct PerInstanceData
			{
				short _posPatch[4];
				short _layers[4];
			};

		private:
			static CGI::ShaderPwn s_shader;
			static UB_DrawDepth   s_ubDrawDepth;

			CGI::GeometryPwn     _geo;
			Vector<TerrainPatch> _vPatches;
			int                  _mapSide = 0;
			int                  _mapShift = 0;
			int                  _numVisiblePatches = 0;
			int                  _numVerts = 0;
			int                  _numIndices = 0;
			TerrainLOD           _lods[5]; // Level of detail data for (16x16, 8x8, 4x4, 2x2, 1x1).

		public:
			Terrain();
			virtual ~Terrain();

			static void InitStatic();
			static void DoneStatic();

			void Init();
			void Done();

			void Layout();
			void Draw(Scene::RcCamera cam);

			int GetMapSide() const { return _mapSide; }

			RTerrainPatch GetPatch(int index) { return _vPatches[index]; }
		};
		VERUS_TYPEDEFS(Terrain);
	}
}
