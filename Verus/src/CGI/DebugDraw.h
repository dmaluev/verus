#pragma once

namespace verus
{
	namespace CGI
	{
		class DebugDraw : public Singleton<DebugDraw>, public Object
		{
			//#include "../Shaders/DR.inc.cg"

			enum SB
			{
				SB_MASTER,
				SB_NO_Z,
				SB_MAX
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

			GeometryPwn _geo;
			ShaderPwn   _shader;
			Vertex*     _pVB = nullptr;
			const int   _maxNumVert = 4096;
			int         _numVert = 0;
			Type        _type = Type::points;

		public:
			DebugDraw();
			~DebugDraw();

			void Init();
			void Done();

			void Begin(Type type, PcTransform3 pMat = nullptr, bool zEnable = true);
			void End();

			void AddPoint(
				RcPoint3 pos,
				UINT32 color);
			void AddLine(
				RcPoint3 posA,
				RcPoint3 posB,
				UINT32 color);
			void AddTriangle(
				RcPoint3 posA,
				RcPoint3 posB,
				RcPoint3 posC,
				UINT32 color);
		};
		VERUS_TYPEDEFS(DebugDraw);
	}
}
