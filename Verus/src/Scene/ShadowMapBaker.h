// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Scene
	{
		class ShadowMapBaker : public Object
		{
		public:
			struct Config
			{
				float _penumbraScale = 0;
				float _penumbraLamBiasScale = 4;
				float _normalDepthBias = 0.012f;
				float _csmContrastScale = 2.8f;
			};
			VERUS_TYPEDEFS(Config);

		protected:
			Matrix4         _matShadow;
			Matrix4         _matShadowDS; // For WV positions in Deferred Shading.
			Config          _config;
			CGI::TexturePwn _tex;
			Camera          _camera;
			PCamera         _pPrevCamera = nullptr;
			int             _side = 0;
			CGI::RPHandle   _rph;
			CGI::FBHandle   _fbh;
			bool            _snapToTexels = true;

		public:
			ShadowMapBaker();
			~ShadowMapBaker();

			void Init(int side);
			void Done();

			void UpdateMatrixForCurrentView();

			void SetSnapToTexels(bool b) { _snapToTexels = b; }
			bool IsBaking() const { return !!_pPrevCamera; }

			CGI::RPHandle GetRenderPassHandle() const { return _rph; }

			void Begin(RcVector3 dirToSun);
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
			Matrix4 _matScreenVP;
			Matrix4 _matScreenP;
			Vector4 _splitRanges = Vector4(0);
			Camera  _cameraCSM;
			int     _currentSplit = -1;
			float   _depth = 0;

		public:
			CascadedShadowMapBaker();
			~CascadedShadowMapBaker();

			void Init(int side);
			void Done();

			void UpdateMatrixForCurrentView();

			void Begin(RcVector3 dirToSun, int split);
			void End(int split);

			RcMatrix4 GetShadowMatrix(int split = 0) const;
			RcMatrix4 GetShadowMatrixDS(int split = 0) const;

			RcMatrix4 GetScreenMatrixVP() const { return _matScreenVP; }
			RcMatrix4 GetScreenMatrixP() const { return _matScreenP; }

			int GetCurrentSplit() const { return _currentSplit; }
			RcVector4 GetSplitRanges() const { return _splitRanges; }

			PCamera GetCameraCSM();
		};
		VERUS_TYPEDEFS(CascadedShadowMapBaker);
	}
}
