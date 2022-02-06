// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class LightMapBaker : public Singleton<LightMapBaker>, public Object, public Math::QuadtreeDelegate
		{
			struct Vertex
			{
				glm::vec3 _pos;
				glm::vec3 _nrm;
				glm::vec2 _tc;
			};
			VERUS_TYPEDEFS(Vertex);

			struct Face
			{
				Vertex _v[3];
			};
			VERUS_TYPEDEFS(Face);

		public:
			enum TEX
			{
				TEX_COLOR,
				TEX_DEPTH,
				TEX_COUNT
			};

			enum class Mode : int
			{
				idle,
				faces,
				ambientOcclusion
			};

			struct Desc
			{
				PcBaseMesh _pMesh = nullptr;
				CSZ        _pathname = nullptr;
				Mode       _mode = Mode::idle;
				int        _texCoordIndex = 0;
				int        _texWidth = 256;
				int        _texHeight = 256;
				int        _texLumelSide = 256;
				float      _distance = 1;
			};
			VERUS_TYPEDEFS(Desc);

		private:
			Math::Quadtree              _quadtree;
			Vector<Face>                _vFaces;
			Vector<UINT32>              _vMap;
			String                      _pathname;
			Desc                        _desc;
			CGI::TexturePwns<TEX_COUNT> _tex;
			CGI::RPHandle               _rph;
			CGI::FBHandle               _fbh;
			int                         _repsPerUpdate = 1;
			int                         _currentI = 0;
			int                         _currentJ = 0;
			glm::vec2                   _currentUV;
			float                       _progress = 0;
			float                       _invMapSize = 0;

		public:
			LightMapBaker();
			~LightMapBaker();

			void Init(RcDesc desc);
			void Done();

			void Update();

			void DrawLumel(RcPoint3 eyePos, RcVector3 frontDir);

			bool IsBaking() const { return Mode::idle != _desc._mode; }
			Mode GetMode() const { return _desc._mode; }

			virtual Continue Quadtree_ProcessNode(void* pToken, void* pUser) override;

			Str GetPathname() const { return _C(_pathname); }
			float GetProgress() const { return _progress; }

			void Save();
		};
		VERUS_TYPEDEFS(LightMapBaker);
	}
}
