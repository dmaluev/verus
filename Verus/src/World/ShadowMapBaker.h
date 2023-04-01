// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace World
	{
		class ShadowMapBaker : public Object
		{
		public:
			struct Config
			{
				float _unormDepthScale = 0; // Scale depth buffer values by this (typically zFar-zNear).
				float _wrapDiffuseScale = 4; // How much wrapDiffuse affects penumbra.
				float _normalDepthBias = 0.04f; // Depth bias along normals.
				float _csmContrastScale = 1.3f; // Makes distant slices sharper.
			};
			VERUS_TYPEDEFS(Config);

		protected:
			Matrix4         _matShadow = Matrix4::identity();
			Matrix4         _matShadowDS = Matrix4::identity(); // For WV positions in Deferred Shading.
			Config          _config;
			CGI::TexturePwn _tex;
			Camera          _passCamera;
			PCamera         _pPrevPassCamera = nullptr;
			int             _side = 0;
			CGI::RPHandle   _rph;
			CGI::FBHandle   _fbh;
			bool            _baking = false;
			bool            _snapToTexels = true;

		public:
			ShadowMapBaker();
			~ShadowMapBaker();

			void Init(int side);
			void Done();

			void UpdateMatrixForCurrentView();

			void SetSnapToTexels(bool b) { _snapToTexels = b; }
			bool IsBaking() const { return _baking; }

			CGI::RPHandle GetRenderPassHandle() const { return _rph; }

			void Begin(RcVector3 dirToSun, RcVector3 up = Vector3(0, 1, 0));
			void End();

			RcMatrix4 GetShadowMatrix() const;
			RcMatrix4 GetShadowMatrixDS() const;

			RConfig GetConfig() { return _config; }

			CGI::TexturePtr GetTexture() const;
		};
		VERUS_TYPEDEFS(ShadowMapBaker);

		class CascadedShadowMapBaker : public ShadowMapBaker
		{
			Matrix4 _matShadowCSM[4];
			Matrix4 _matShadowCSM_DS[4]; // For WV positions in Deferred Shading.
			Matrix4 _matOffset[4];
			Matrix4 _matScreenVP = Matrix4::identity();
			Matrix4 _matScreenP = Matrix4::identity();
			Vector4 _sliceBounds = Vector4(0);
			int     _currentSlice = -1;
			float   _depth = 0;
			float   _headCameraPreferredRange = 0;

		public:
			CascadedShadowMapBaker();
			~CascadedShadowMapBaker();

			void Init(int side);
			void Done();

			void UpdateMatrixForCurrentView();

			void Begin(RcVector3 dirToSun, int slice, RcVector3 up = Vector3(0, 1, 0));
			void End(int slice);

			RcMatrix4 GetShadowMatrix(int slice = 0) const;
			RcMatrix4 GetShadowMatrixDS(int slice = 0) const;

			RcMatrix4 GetScreenMatrixVP() const { return _matScreenVP; }
			RcMatrix4 GetScreenMatrixP() const { return _matScreenP; }

			int GetCurrentSlice() const { return _currentSlice; }
			RcVector4 GetSliceBounds() const { return _sliceBounds; }
		};
		VERUS_TYPEDEFS(CascadedShadowMapBaker);
	}
}
