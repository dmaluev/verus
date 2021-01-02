// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class CubeMap : public Object
		{
		public:
			enum TEX
			{
				TEX_COLOR,
				TEX_DEPTH,
				TEX_COUNT
			};

		private:
			Point3                      _center;
			CGI::TexturePwns<TEX_COUNT> _tex;
			CGI::RPHandle               _rph;
			CGI::FBHandle               _fbh[+CGI::CubeMapFace::count];
			MainCamera                  _camera;
			PMainCamera                 _pPrevCamera = nullptr;
			int                         _side = 0;

		public:
			CubeMap();
			~CubeMap();

			void Init(int side);
			void Done();

			CGI::RPHandle GetRenderPassHandle() const { return _rph; }
			void BeginEnvMap(CGI::CubeMapFace cubeMapFace, RcPoint3 center);
			void EndEnvMap();

			CGI::TexturePtr GetColorTexture();
			CGI::TexturePtr GetDepthTexture();

			RcPoint3 GetCenter() const { return _center; }
		};
		VERUS_TYPEDEFS(CubeMap);
	}
}
