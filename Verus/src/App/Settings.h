#pragma once

namespace verus
{
	namespace App
	{
		class Limits
		{
		public:
			int _d3d12_dhCbvSrvUavCapacity = 10000;
			int _d3d12_dhSamplersCapacity = 500;
			int _mesh_ubPerFrameCapacity = 100;
			int _mesh_ubPerMaterialFSCapacity = 1000;
			int _mesh_ubPerMeshVSCapacity = 10000;
			int _mesh_ubSkinningVSCapacity = 5000;
			int _terrain_ubDrawDepthCapacity = 100;
		};
		VERUS_TYPEDEFS(Limits);

		class Settings : public Singleton<Settings>, public IO::Json
		{
		public:
			struct CommandLine
			{
				bool _normalMapOnlyLS;
				bool _physicsDebug;
				bool _physicsDebugFull;
				bool _profiler;
				bool _testTexToPix;
			};

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
				nearest,
				linear,
				multisampled,
				cascaded,
				ultra
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
			int           _gapi = 0;
			int           _gpuAnisotropyLevel = 4;
			int           _gpuAntialiasingLevel = 0;
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
			ShadowQuality _sceneShadowQuality = ShadowQuality::multisampled;
			WaterQuality  _sceneWaterQuality = WaterQuality::solidColor;
			float         _screenFOV = 70;
			int           _screenSizeHeight = 720;
			int           _screenSizeWidth = 1280;
			bool          _screenVSync = false;
			bool          _screenWindowed = true;
			String        _uiLang = "EN";
			CommandLine   _commandLine;
			Limits        _limits;
			String        _imguiFont;

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

			RcLimits GetLimits() const { return _limits; }
		};
		VERUS_TYPEDEFS(Settings);
	}
}
