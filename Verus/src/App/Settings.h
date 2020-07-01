#pragma once

namespace verus
{
	namespace App
	{
		class Limits
		{
		public:
			int _d3d12_dhViewsCapacity = 10000;
			int _d3d12_dhSamplersCapacity = 500;
			int _mesh_ubPerFrameCapacity = 100;
			int _mesh_ubPerMaterialFSCapacity = 1000;
			int _mesh_ubPerMeshVSCapacity = 10000;
			int _mesh_ubSkinningVSCapacity = 5000;
			int _terrain_ubDrawDepthCapacity = 100;
		};
		VERUS_TYPEDEFS(Limits);

		class Settings : public Singleton<Settings>, IO::Json
		{
			typedef Map<String, String> TMapLocalizedStrings;

			TMapLocalizedStrings _mapLocalizedStrings;

		public:
			struct CommandLine
			{
				int _gapi = -1;
				bool _fullscreen = false;
				bool _windowed = false;
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
			bool          _gpuTessellation = true;
			int	          _gpuTextureLodLevel = 0;
			bool          _gpuTrilinearFilter = true;
			float         _inputMouseSensitivity = 1;
			bool          _physicsSupportDebugDraw = false;
			bool          _postProcessBloom = false;
			bool          _postProcessCinema = false;
			bool          _postProcessMotionBlur = false;
			bool          _postProcessSSAO = false;
			int	          _sceneGrassDensity = 1000;
			ShadowQuality _sceneShadowQuality = ShadowQuality::multisampled;
			WaterQuality  _sceneWaterQuality = WaterQuality::solidColor;
			bool          _screenAllowHighDPI = true;
			float         _screenFOV = 70;
			bool          _screenOffscreenDraw = true;
			int           _screenSizeHeight = 720;
			int           _screenSizeWidth = 1280;
			bool          _screenVSync = true;
			bool          _screenWindowed = true;
			String        _uiLang = "EN";
			CommandLine   _commandLine;
			Limits        _limits;
			String        _imguiFont;
			float         _highDpiScale = 1;

			Settings();
			~Settings();

			void ParseCommandLineArgs(int argc, wchar_t* argv[]);
			void ParseCommandLineArgs(int argc, char* argv[]);
			void SetQuality(Quality q);

			void Load();
			void Validate();
			void Save();

			void MatchScreen();

			RcLimits GetLimits() const { return _limits; }

			void LoadLocalizedStrings(CSZ url);
			CSZ GetLocalizedString(CSZ id) const;

			void UpdateHighDpiScale();
			int GetFontSize() const;
		};
		VERUS_TYPEDEFS(Settings);
	}
}
