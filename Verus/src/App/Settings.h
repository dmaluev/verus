// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::App
{
	enum class DisplayMode : int
	{
		exclusiveFullscreen,
		windowed,
		borderlessWindowed
	};

	class Info
	{
	public:
		CSZ    _appName = nullptr;
		UINT32 _appVersion = 0;
		CSZ    _engineName = nullptr;
		UINT32 _engineVersion = 0;
	};
	VERUS_TYPEDEFS(Info);

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
		int          _gpuAntiAliasingLevel = 0;
		Quality      _gpuShaderQuality = Quality::medium;
		bool         _gpuTessellation = false;
		int	         _gpuTextureLodLevel = 0;
		bool         _gpuTrilinearFilter = true;
		bool         _postProcessAntiAliasing = false;
		bool         _postProcessBloom = true;
		bool         _postProcessCinema = false;
		bool         _postProcessLightShafts = true;
		bool         _postProcessMotionBlur = false;
		bool         _postProcessSSAO = true;
		bool         _postProcessSSR = false;
		bool         _sceneAmbientOcclusion = true;
		int	         _sceneGrassDensity = 600;
		Quality      _sceneShadowQuality = Quality::medium;
		WaterQuality _sceneWaterQuality = WaterQuality::solidColor;

		bool operator==(const QualitySettings& that) const;
		bool operator!=(const QualitySettings& that) const;

		void SetQuality(OverallQuality q);
		void SetXrQuality();
		OverallQuality DetectQuality() const;
	};
	VERUS_TYPEDEFS(QualitySettings);

	// Capacity is per frame. 3 frames are buffered.
	class Limits
	{
	public:
		int _d3d12_dhViewsCapacity = 50000; // D3D limit is one million.
		int _d3d12_dhSamplersCapacity = 500; // D3D limit is 2048.
		int _ds_ubViewCapacity = 20;
		int _ds_ubSubpassFSCapacity = 20;
		int _ds_ubShadowFSCapacity = 50;
		int _ds_ubMeshVSCapacity = 20;
		int _forest_ubVSCapacity = 200;
		int _forest_ubFSCapacity = 200;
		int _generateMips_ubCapacity = 50;
		int _generateCubeMapMips_ubCapacity = 50;
		int _grass_ubVSCapacity = 20;
		int _grass_ubFSCapacity = 20;
		int _gui_ubGuiCapacity = 100;
		int _gui_ubGuiFSCapacity = 100;
		int _mesh_ubViewCapacity = 800;
		int _mesh_ubMaterialFSCapacity = 1000;
		int _mesh_ubMeshVSCapacity = 2000;
		int _mesh_ubSkeletonVSCapacity = 200;
		int _particles_ubVSCapacity = 100;
		int _particles_ubFSCapacity = 100;
		int _quad_ubVSCapacity = 20;
		int _quad_ubFSCapacity = 20;
		int _sky_ubViewCapacity = 80;
		int _sky_ubMaterialFSCapacity = 80;
		int _sky_ubMeshVSCapacity = 80;
		int _sky_ubObjectCapacity = 80;
		int _terrain_ubVSCapacity = 40;
		int _terrain_ubFSCapacity = 40;
		int _water_ubVSCapacity = 20;
		int _water_ubFSCapacity = 20;
	};
	VERUS_TYPEDEFS(Limits);

	class Settings : public QualitySettings, public Singleton<Settings>, IO::Json
	{
		typedef Map<String, String> TMapLocalizedStrings;

		TMapLocalizedStrings _mapLocalizedStrings;

	public:
		struct CommandLine
		{
			int   _gapi = -1;
			int   _openXR = -1;
			float _xrHeight = -FLT_MAX;
			bool  _exclusiveFullscreen = false;
			bool  _windowed = false;
			bool  _borderlessWindowed = false;
			bool  _restarted = false;
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
		bool        _openXR = false;
		bool        _physicsSupportDebugDraw = false;
		String      _uiLang = "EN";
		float       _xrFOV = 110;
		float       _xrHeight = 0;
		CommandLine _commandLine;
		Limits      _limits;
		String      _imguiFont;
		Info        _info;
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

		int Scale(int size, float extraScale = 1) const;
		float GetScale() const;
	};
	VERUS_TYPEDEFS(Settings);
}
