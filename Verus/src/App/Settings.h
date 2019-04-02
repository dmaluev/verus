#pragma once

namespace verus
{
	namespace App
	{
		class Settings : public Singleton<Settings>, public IO::Json
		{
		public:
			enum class Quality : int
			{
				low,
				medium,
				high,
				ultra
			};

			enum class ShadowQuality : int
			{
				none,
				sharp,
				filtered,
				cascaded
			};

			enum class WaterQuality : int
			{
				solidColor,
				simpleReflection,
				distortedReflection,
				trueWavesReflection,
				trueWavesRefraction
			};

			Quality       _quality = Quality::medium;
			int           _gapi = 12;
			int           _gpuAnisotropyLevel = 4;
			int           _gpuAntialiasingLevel = 0;
			int           _gpuDepthTexture = 0;
			String        _gpuForcedProfile = "sm_3_0";
			bool          _gpuOffscreenRender = false;
			bool          _gpuParallaxMapping = false;
			bool          _gpuPerPixelLighting = true;
			int	          _gpuTextureLodLevel = 0;
			bool          _gpuTrilinearFilter = false;
			float         _inputMouseSensitivity = 1;
			bool          _postProcessBloom = false;
			bool          _postProcessCinema = false;
			bool          _postProcessMotionBlur = false;
			bool          _postProcessSSAO = false;
			int	          _sceneGrassDensity = 1000;
			ShadowQuality _sceneShadowQuality = ShadowQuality::sharp;
			WaterQuality  _sceneWaterQuality = WaterQuality::solidColor;
			float         _screenFOV = 70;
			int           _screenSizeHeight = 720;
			int           _screenSizeWidth = 1280;
			bool          _screenVSync = false;
			bool          _screenWindowed = true;
			String        _uiLang = "EN";
			struct
			{
				bool normalMapOnlyLS;
				bool physicsDebug;
				bool physicsDebugFull;
				bool profiler;
				bool testTexToPix;
			} _commandLine;

			Settings();
			~Settings();

			void LoadValidateSave();
			void Load();
			void Save();
			void Validate();

			void ParseCommandLineArgs(int argc, wchar_t* argv[]);
			void ParseCommandLineArgs(int argc, char* argv[]);

			void SetQuality(Quality q);

			void MatchScreen();
		};
		VERUS_TYPEDEFS(Settings);
	}
}
