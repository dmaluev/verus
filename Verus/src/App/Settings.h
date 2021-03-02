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

		class Settings : public Singleton<Settings>, IO::Json
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

			enum Platform : int
			{
				classic,
				uwp
			};

			Quality       _quality = Quality::medium;
			bool          _displayAllowHighDPI = true;
			float         _displayFOV = 70;
			DisplayMode   _displayMode = DisplayMode::windowed;
			bool          _displayOffscreenDraw = true;
			float         _displayOffscreenScale = 1;
			int           _displaySizeHeight = 720;
			int           _displaySizeWidth = 1280;
			bool          _displayVSync = true;
			int           _gapi = 0;
			int           _gpuAnisotropyLevel = 4;
			int           _gpuAntialiasingLevel = 0;
			Quality       _gpuShaderQuality = Quality::medium;
			bool          _gpuTessellation = false;
			int	          _gpuTextureLodLevel = 0;
			bool          _gpuTrilinearFilter = true;
			float         _inputMouseSensitivity = 1;
			bool          _physicsSupportDebugDraw = false;
			bool          _postProcessBloom = true;
			bool          _postProcessCinema = false;
			bool          _postProcessGodRays = true;
			bool          _postProcessMotionBlur = false;
			bool          _postProcessSSAO = true;
			bool          _postProcessSSR = false;
			bool          _sceneAmbientOcclusion = false;
			int	          _sceneGrassDensity = 800;
			Quality       _sceneShadowQuality = Quality::medium;
			WaterQuality  _sceneWaterQuality = WaterQuality::solidColor;
			String        _uiLang = "EN";
			CommandLine   _commandLine;
			Limits        _limits;
			String        _imguiFont;
			float         _highDpiScale = 1;
			Platform      _platform = Platform::classic;

			Settings();
			~Settings();

			void ParseCommandLineArgs(int argc, wchar_t* argv[]);
			void ParseCommandLineArgs(int argc, char* argv[]);
			void SetQuality(Quality q);

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
