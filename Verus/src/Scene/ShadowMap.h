#pragma once

namespace verus
{
	namespace Scene
	{
		class ShadowMap : public Object
		{
		public:
			struct Config
			{
				float _penumbraScale = 0;
				float _penumbraLamBiasScale = 2.5f;
				float _normalDepthBias = 0.012f;
				float _csmContrastScale = 2.8f;
			};
			VERUS_TYPEDEFS(Config);

		protected:
			Matrix4         _matShadow;
			Matrix4         _matShadowDS;
			Config          _config;
			CGI::TexturePwn _tex;
			Camera          _camera;
			PCamera         _pSceneCamera = nullptr;
			int             _side = 0;
			CGI::RPHandle   _rph;
			CGI::FBHandle   _fbh;
			bool            _snapToTexels = true;
			bool            _rendering = false;

		public:
			ShadowMap();
			~ShadowMap();

			void Init(int side);
			void Done();

			void SetSnapToTexels(bool b) { _snapToTexels = b; }
			bool IsRendering() const { return _rendering; }

			CGI::RPHandle GetRenderPassHandle() const { return _rph; }

			void Begin(RcVector3 dirToSun, float zNear = 1, float zFar = 0);
			void BeginLight(RcPoint3 pos, RcPoint3 target, float side = 10);
			void End();

			RcMatrix4 GetShadowMatrix() const;
			RcMatrix4 GetShadowMatrixDS() const;

			RConfig GetConfig() { return _config; }

			CGI::TexturePtr GetTexture() const;

			PCamera GetSceneCamera() { return _pSceneCamera; }
		};
		VERUS_TYPEDEFS(ShadowMap);

		class CascadedShadowMap : public ShadowMap
		{
			Matrix4 _matShadowCSM[4];
			Matrix4 _matShadowCSM_DS[4];
			Matrix4 _matOffset[4];
			Vector4 _splitRanges = Vector4(0);
			Camera  _cameraCSM;
			int     _currentSplit = -1;

		public:
			CascadedShadowMap();
			~CascadedShadowMap();

			void Init(int side);
			void Done();

			void Begin(RcVector3 dirToSun, float zNear, float zFar, int split);
			void End(int split);

			RcMatrix4 GetShadowMatrix(int split = 0) const;
			RcMatrix4 GetShadowMatrixDS(int split = 0) const;

			int GetCurrentSplit() const { return _currentSplit; }
			RcVector4 GetSplitRanges() const { return _splitRanges; }

			PCamera GetCameraCSM();
		};
		VERUS_TYPEDEFS(CascadedShadowMap);
	}
}
