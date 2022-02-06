// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		// Managed by DeferredShading:
		class DeferredLights
		{
			Mesh _meshDir;
			Mesh _meshOmni;
			Mesh _meshSpot;

		public:
			RMesh Get(CGI::LightType type)
			{
				switch (type)
				{
				case CGI::LightType::omni: return _meshOmni;
				case CGI::LightType::spot: return _meshSpot;
				}
				return _meshDir;
			}
		};
		VERUS_TYPEDEFS(DeferredLights);

		class Helpers : public Singleton<Helpers>, public Object
		{
		private:
			struct Vertex
			{
				Point3 _pos;
				UINT32 _color;

				Vertex() {}
				Vertex(RcPoint3 pos, UINT32 color)
				{
					_pos = pos;
					_color = color;
				}
			};

			Vector<Vertex> _vGrid;
			Vector<Vertex> _vBasis;
			Vector<Vertex> _vCircle;
			Vector<Vertex> _vBox;
			Vector<Vertex> _vLight;
			Mesh           _sphere;
			DeferredLights _deferredLights;

		public:
			Helpers();
			~Helpers();

			void Init();
			void Done();

			void DrawGrid();
			void  DrawBasis(PcTransform3 pMat = nullptr, int axis = -1, bool overlay = false);
			void DrawCircle(RcPoint3 pos, float radius, UINT32 color, RTerrain terrain);
			void    DrawBox(PcTransform3 pMat = nullptr, UINT32 color = 0);
			void  DrawLight(RcPoint3 pos, UINT32 color = 0, PcPoint3 pTarget = nullptr);
			void DrawSphere(RcPoint3 pos, float r, UINT32 color, CGI::CommandBufferPtr cb);

			static UINT32 GetBasisColorX(bool linear = false, int alpha = 255);
			static UINT32 GetBasisColorY(bool linear = false, int alpha = 255);
			static UINT32 GetBasisColorZ(bool linear = false, int alpha = 255);

			RDeferredLights GetDeferredLights() { return _deferredLights; }
		};
		VERUS_TYPEDEFS(Helpers);
	}
}
