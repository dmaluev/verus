// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace App
	{
		enum class DisplayMode : int
		{
			exclusiveFullscreen,
			windowed,
			borderlessWindowed
		};

		class QualitySettings
		{
		public:
			enum class OverallQuality : int
			{
				custom,
				low,
				medium,
				high,
				ultra
			};

			enum class Quality : int
			{
				low,
				medium,
				high,
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

		public:
			bool         _displayOffscreenDraw = true;
			float        _displayOffscreenScale = 1;
			int          _gpuAnisotropyLevel = 4;
			int          _gpuAntialiasingLevel = 0;
			Quality      _gpuShaderQuality = Quality::medium;
			bool         _gpuTessellation = false;
			int	         _gpuTextureLodLevel = 0;
			bool         _gpuTrilinearFilter = true;
			bool         _postProcessBloom = true;
			bool         _postProcessCinema = false;
			bool         _postProcessLightShafts = true;
			bool         _postProcessMotionBlur = false;
			bool         _postProcessSSAO = true;
			bool         _postProcessSSR = false;
			bool         _sceneAmbientOcclusion = false;
			int	         _sceneGrassDensity = 800;
			Quality      _sceneShadowQuality = Quality::medium;
			WaterQuality _sceneWaterQuality = WaterQuality::solidColor;

			bool operator==(const QualitySettings& that) const;

			void SetQuality(OverallQuality q);
			OverallQuality DetectQuality() const;
		};
		VERUS_TYPEDEFS(QualitySettings);

		// Capacity is per frame. 3 frames are buffered.
		class Limits
		{
		public:
			int _d3d12_dhViewsCapacity = 10000; // D3D limit is one million.
			int _d3d12_dhSamplersCapacity = 500; // D3D limit is 2048.
			int _ds_ubPerFrameCapacity = 4;
			int _ds_ubTexturesFSCapacity = 4;
			int _ds_ubPerMeshVSCapacity = 4;
			int _ds_ubShadowFSCapacity = 4;
			int _ds_ubAOPerFrameCapacity = 2;
			int _ds_ubAOTexturesFSCapacity = 2;
			int _ds_ubAOPerMeshVSCapacity = 10;
			int _generateMips_ubCapacity = 50;
			int _grass_ubVSCapacity = 4;
			int _grass_ubFSCapacity = 4;
			int _gui_ubGuiCapacity = 100;
			int _gui_ubGuiFSCapacity = 100;
			int _mesh_ubPerFrameCapacity = 200;
			int _mesh_ubPerMaterialFSCapacity = 1000;
			int _mesh_ubPerMeshVSCapacity = 2000;
			int _mesh_ubSkinningVSCapacity = 200;
			int _particles_ubVSCapacity = 100;
			int _particles_ubFSCapacity = 100;
			int _quad_ubVSCapacity = 20;
			int _quad_ubFSCapacity = 20;
			int _sky_ubPerFrameCapacity = 50;
			int _sky_ubPerMaterialFSCapacity = 50;
			int _sky_ubPerMeshVSCapacity = 50;
			int _sky_ubPerObjectCapacity = 50;
			int _terrain_ubVSCapacity = 20;
			int _terrain_ubFSCapacity = 20;
			int _water_ubVSCapacity = 2;
			int _water_ubFSCapacity = 2;
		};
		VERUS_TYPEDEFS(Limits);

		class Settings : public QualitySettings, public Singleton<Settings>, IO::Json
		{
			typedef Map<String, String> TMapLocalizedStrings;

			TMapLocalizedStrings _mapLocalizedStrings;

		public:
			struct CommandLine
			{
				int  _gapi = -1;
				bool _exclusiveFullscreen = false;
				bool _windowed = false;
				bool _borderlessWindowed = false;
				bool _restarted = false;
			};

			enum Platform : int
			{
				classic,
				uwp
			};

			bool        _displayAllowHighDPI = true;
			float       _displayFOV = 70;
			DisplayMode _displayMode = DisplayMode::windowed;
			int         _displaySizeHeight = 720;
			int         _displaySizeWidth = 1280;
			bool        _displayVSync = true;
			int         _gapi = 0;
			float       _inputMouseSensitivity = 1;
			bool        _physicsSupportDebugDraw = false;
			String      _uiLang = "EN";
			CommandLine _commandLine;
			Limits      _limits;
			String      _imguiFont;
			float       _highDpiScale = 1;
			Platform    _platform = Platform::classic;

			Settings();
			~Settings();

			void ParseCommandLineArgs(int argc, wchar_t* argv[]);
			void ParseCommandLineArgs(int argc, char* argv[]);

			void Load();
			void HandleHighDpi();
			void HandleCommandLineArgs();
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
