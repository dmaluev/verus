// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::World
{
	class EditorOverlays : public Singleton<EditorOverlays>, public Object
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

		Vector<Vertex> _vBasis;
		Vector<Vertex> _vBox;
		Vector<Vertex> _vCircle;
		Vector<Vertex> _vGrid;
		Vector<Vertex> _vLight;
		Mesh           _sphere;

	public:
		EditorOverlays();
		~EditorOverlays();

		void Init();
		void Done();

		void         DrawBasis(PcTransform3 pMat = nullptr, int axis = -1, bool depthTestEnable = true, UINT32 middleColor = 0);
		void           DrawBox(PcTransform3 pMat = nullptr, UINT32 color = 0);
		void        DrawCircle(PcTransform3 pMat, float radius, UINT32 color);
		void DrawTerrainCircle(RcPoint3 pos, float radius, UINT32 color, RTerrain terrain);
		void          DrawGrid();
		void         DrawLight(RcPoint3 pos, UINT32 color = 0, PcPoint3 pTarget = nullptr);
		void        DrawSphere(RcPoint3 pos, float r, UINT32 color, CGI::CommandBufferPtr cb);

		static UINT32 GetBasisColorX(bool linear = false, int alpha = 255);
		static UINT32 GetBasisColorY(bool linear = false, int alpha = 255);
		static UINT32 GetBasisColorZ(bool linear = false, int alpha = 255);
		static UINT32 GetLightColor(int alpha = 255);
	};
	VERUS_TYPEDEFS(EditorOverlays);
}
