// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		struct DrawLumelDesc
		{
			Transform3 _matV;
			Point3     _eyePos;
			Vector3    _frontDir;
		};
		VERUS_TYPEDEFS(DrawLumelDesc);

		struct LightMapBakerDelegate
		{
			virtual void LightMapBaker_Draw(CGI::CubeMapFace cubeMapFace, RcDrawLumelDesc drawLumelDesc) = 0;
		};
		VERUS_TYPEDEFS(LightMapBakerDelegate);

		class LightMapBaker : public Singleton<LightMapBaker>, public Object, public Math::QuadtreeDelegate
		{
			static const int s_batchSize = 400;
			static const int s_maxLayers = 8;

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

			struct Queued
			{
				Point3  _pos;
				Vector3 _nrm;
				void* _pDst;
			};
			VERUS_TYPEDEFS(Queued);

		public:
			enum PIPE
			{
				PIPE_MESH_SIMPLE_BAKE_AO,
				PIPE_MESH_SIMPLE_BAKE_AO_INSTANCED,
				PIPE_MESH_SIMPLE_BAKE_AO_ROBOTIC,
				PIPE_MESH_SIMPLE_BAKE_AO_SKINNED,
				PIPE_HEMICUBE_MASK,
				PIPE_QUAD,
				PIPE_COUNT
			};

			enum TEX
			{
				TEX_DEPTH,
				TEX_DUMMY,
				TEX_COUNT
			};

			enum class Mode : int
			{
				idle,
				faces,
				ambientOcclusion
			};

			struct Stats
			{
				String _info;
				float  _progress = 0;
				float  _startTime = 0;
				int    _maxLayer = 0;
			};
			VERUS_TYPEDEFS(Stats);

			struct Desc
			{
				PcMesh _pMesh = nullptr;
				CSZ    _pathname = nullptr;
				Mode   _mode = Mode::idle;
				int    _texCoordSet = 0;
				int    _texWidth = 256;
				int    _texHeight = 256;
				int    _texLumelSide = 128;
				float  _distance = 2;
				float  _bias = 0.001f;
			};
			VERUS_TYPEDEFS(Desc);

		private:
			Math::Quadtree                _quadtree;
			PLightMapBakerDelegate        _pDelegate = nullptr;
			Vector<Face>                  _vFaces;
			Vector<UINT32>                _vMap;
			Vector<Queued>                _vQueued[CGI::BaseRenderer::s_ringBufferSize];
			String                        _pathname;
			Desc                          _desc;
			CGI::PipelinePwns<PIPE_COUNT> _pipe;
			CGI::TexturePwns<TEX_COUNT>   _tex;
			CGI::TexturePwns<s_batchSize> _texColor[CGI::BaseRenderer::s_ringBufferSize];
			CGI::RPHandle                 _rph;
			CGI::FBHandle                 _fbh[CGI::BaseRenderer::s_ringBufferSize][s_batchSize];
			CGI::CSHandle                 _cshQuad[CGI::BaseRenderer::s_ringBufferSize];
			CGI::CSHandle                 _cshHemicubeMask;
			int                           _repsPerUpdate = 1;
			int                           _drawEmptyState = 0;
			int                           _queuedCount[CGI::BaseRenderer::s_ringBufferSize];
			int                           _currentLayer = 0;
			int                           _currentI = 0;
			int                           _currentJ = 0;
			glm::vec2                     _currentUV;
			float                         _invMapSize = 0;
			float                         _normalizationFactor = 0;
			Stats                         _stats;
			bool                          _debugDraw = false;

		public:
			LightMapBaker();
			~LightMapBaker();

			void Init(RcDesc desc);
			void Done();

			void Update();
			void Draw();

			PLightMapBakerDelegate SetDelegate(PLightMapBakerDelegate p) { return Utils::Swap(_pDelegate, p); }

			void DrawEmpty();
			void DrawHemicubeMask();
			void DrawLumel(RcPoint3 pos, RcVector3 nrm, int batchIndex);

			RcDesc GetDesc() const { return _desc; }

			bool IsBaking() const { return Mode::idle != _desc._mode; }
			Mode GetMode() const { return _desc._mode; }

			bool IsDebugDrawEnabled() const { return _debugDraw; }
			void EnableDebugDraw(bool b = true) { _debugDraw = b; }

			virtual Continue Quadtree_ProcessNode(void* pToken, void* pUser) override;

			Str GetPathname() const { return _C(_pathname); }
			RcStats GetStats() const { return _stats; }
			float GetProgress() const { return _stats._progress; }

			void ComputeEdgePadding();
			void Save();

			void BindPipeline(RcMesh mesh, CGI::CommandBufferPtr cb);
			void SetViewportFor(CGI::CubeMapFace cubeMapFace, CGI::CommandBufferPtr cb);
		};
		VERUS_TYPEDEFS(LightMapBaker);
	}
}
