// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace CGI
	{
		class DebugDraw : public Singleton<DebugDraw>, public Object
		{
#include "../Shaders/DebugDraw.inc.hlsl"

			enum PIPE
			{
				PIPE_POINTS,
				PIPE_POINTS_NO_Z,
				PIPE_LINES,
				PIPE_LINES_NO_Z,
				PIPE_POLY,
				PIPE_POLY_NO_Z,
				PIPE_COUNT
			};

		public:
			enum class Type : int
			{
				points,
				lines,
				poly
			};

		private:
			struct Vertex
			{
				float _pos[3];
				BYTE  _color[4];
			};

			static UB_DebugDraw s_ubDebugDraw;

			GeometryPwn              _geo;
			ShaderPwn                _shader;
			PipelinePwns<PIPE_COUNT> _pipe;
			Vector<Vertex>           _vDynamicBuffer;
			UINT64                   _currentFrame = UINT64_MAX;
			const int                _maxVerts = 0x10000;
			int                      _vertCount = 0;
			int                      _offset = 0;
			int                      _peakLoad = 0;
			Type                     _type = Type::points;

		public:
			DebugDraw();
			~DebugDraw();

			void Init();
			void Done();

			void Begin(Type type, PcTransform3 pMat = nullptr, bool zEnable = true);
			void End();

			bool AddPoint(
				RcPoint3 pos,
				UINT32 color);
			bool AddLine(
				RcPoint3 posA,
				RcPoint3 posB,
				UINT32 color);
			bool AddTriangle(
				RcPoint3 posA,
				RcPoint3 posB,
				RcPoint3 posC,
				UINT32 color);
		};
		VERUS_TYPEDEFS(DebugDraw);
	}
}
