// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::App;

// QualitySettings:

bool QualitySettings::operator==(RcQualitySettings that) const
{
	return
		_displayOffscreenDraw == that._displayOffscreenDraw &&
		_displayOffscreenScale == that._displayOffscreenScale &&
		_gpuAnisotropyLevel == that._gpuAnisotropyLevel &&
		_gpuAntiAliasingLevel == that._gpuAntiAliasingLevel &&
		_gpuShaderQuality == that._gpuShaderQuality &&
		_gpuTessellation == that._gpuTessellation &&
		_gpuTextureLodLevel == that._gpuTextureLodLevel &&
		_gpuTrilinearFilter == that._gpuTrilinearFilter &&
		_postProcessAntiAliasing == that._postProcessAntiAliasing &&
		_postProcessBloom == that._postProcessBloom &&
		_postProcessCinema == that._postProcessCinema &&
		_postProcessLightShafts == that._postProcessLightShafts &&
		_postProcessMotionBlur == that._postProcessMotionBlur &&
		_postProcessSSAO == that._postProcessSSAO &&
		_postProcessSSR == that._postProcessSSR &&
		_sceneAmbientOcclusion == that._sceneAmbientOcclusion &&
		_sceneGrassDensity == that._sceneGrassDensity &&
		_sceneShadowQuality == that._sceneShadowQuality &&
		_sceneWaterQuality == that._sceneWaterQuality;
}

bool QualitySettings::operator!=(RcQualitySettings that) const
{
	return !(*this == that);
}

void QualitySettings::SetQuality(OverallQuality q)
{
	if (OverallQuality::custom == q)
		return;

	*this = QualitySettings(); // Start with medium.

	switch (q)
	{
	case OverallQuality::low:
		_gpuAnisotropyLevel = 2;
		_gpuShaderQuality = Quality::low;
		_gpuTrilinearFilter = false;
		_postProcessBloom = false;
		_postProcessLightShafts = false;
		_postProcessSSAO = false;
		_sceneGrassDensity = 500;
		_sceneShadowQuality = Quality::low;
		_sceneWaterQuality = WaterQuality::solidColor;
		break;
	case OverallQuality::medium:
		break;
	case OverallQuality::high:
		_gpuAnisotropyLevel = 8;
		_gpuShaderQuality = Quality::high;
		_postProcessCinema = true;
		_sceneAmbientOcclusion = true;
		_sceneGrassDensity = 700;
		_sceneShadowQuality = Quality::high;
		_sceneWaterQuality = WaterQuality::trueWavesReflection;
		break;
	case OverallQuality::ultra:
		_gpuAnisotropyLevel = 16;
		_gpuShaderQuality = Quality::ultra;
		_postProcessAntiAliasing = true;
		_postProcessCinema = true;
		_postProcessMotionBlur = true;
		_postProcessSSR = true;
		_sceneAmbientOcclusion = true;
		_sceneGrassDensity = 800;
		_sceneShadowQuality = Quality::ultra;
		_sceneWaterQuality = WaterQuality::trueWavesRefraction;
		break;
	}
}

void QualitySettings::SetXrQuality()
{
	SetQuality(OverallQuality::high);

	_gpuAnisotropyLevel = 16;
	_postProcessAntiAliasing = true;
	_postProcessCinema = false;
}

QualitySettings::OverallQuality QualitySettings::DetectQuality() const
{
	QualitySettings reference;

	reference.SetQuality(OverallQuality::low);
	if (*this == reference)
		return OverallQuality::low;

	reference.SetQuality(OverallQuality::medium);
	if (*this == reference)
		return OverallQuality::medium;

	reference.SetQuality(OverallQuality::high);
	if (*this == reference)
		return OverallQuality::high;

	reference.SetQuality(OverallQuality::ultra);
	if (*this == reference)
		return OverallQuality::ultra;

	return OverallQuality::custom;
}

// Settings:

Settings::Settings()
{
#ifdef _WIN32
	if (0x419 == GetUserDefaultUILanguage())
		_uiLang = "RU";
#endif
	SetQuality(OverallQuality::medium);
}

Settings::~Settings()
{
}

void Settings::ParseCommandLineArgs(int argc, wchar_t* argv[])
{
	Vector<String> v;
	VERUS_FOR(i, argc)
		v.push_back(Str::WideToUtf8(argv[i]));

	Vector<SZ> vArgv;
	VERUS_FOR(i, argc)
		vArgv.push_back(const_cast<SZ>(_C(v[i])));

	ParseCommandLineArgs(argc, vArgv.data());
}

void Settings::ParseCommandLineArgs(int argc, char* argv[])
{
	auto IsArg = [&argv](int i, CSZ arg)
	{
		return !strcmp(argv[i], arg);
	};

	VERUS_FOR(i, argc)
	{
		if (IsArg(i, "--gapi") && i + 1 < argc)
			_commandLine._gapi = atoi(argv[i + 1]);
		if (IsArg(i, "--openxr") && i + 1 < argc)
			_commandLine._openXR = atoi(argv[i + 1]);
		if (IsArg(i, "--efs") || IsArg(i, "--fullscreen"))
			_commandLine._exclusiveFullscreen = true;
		if (IsArg(i, "--wnd") || IsArg(i, "--windowed"))
			_commandLine._windowed = true;
		if (IsArg(i, "--bwnd"))
			_commandLine._borderlessWindowed = true;
		if (IsArg(i, "--restarted"))
			_commandLine._restarted = true;
		if (IsArg(i, "--xr-height") && i + 1 < argc)
			_commandLine._xrHeight = static_cast<float>(atof(argv[i + 1]));
	}

	SetFilename("Settings.json");
	const String pathname = _C(GetFilename());

	VERUS_FOR(i, argc)
	{
		if (IsArg(i, "--q-low"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(OverallQuality::low);
		}
		if (IsArg(i, "--q-medium"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(OverallQuality::medium);
		}
		if (IsArg(i, "--q-high"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(OverallQuality::high);
		}
		if (IsArg(i, "--q-ultra"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(OverallQuality::ultra);
		}

		if (IsArg(i, "--q-xr"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			_commandLine._gapi = 12;
			_commandLine._windowed = true;
			SetXrQuality();
		}
	}
}

void Settings::Load()
{
	Json::Load();

	_displayAllowHighDPI = GetB("displayAllowHighDPI", _displayAllowHighDPI);
	_displayFOV = GetF("displayFOV", _displayFOV);
	_displayMode = static_cast<DisplayMode>(GetI("displayMode", +_displayMode));
	_displayOffscreenDraw = GetB("displayOffscreenDraw", _displayOffscreenDraw);
	_displayOffscreenScale = GetF("displayOffscreenScale", _displayOffscreenScale);
	_displaySizeHeight = GetI("displaySizeHeight", _displaySizeHeight);
	_displaySizeWidth = GetI("displaySizeWidth", _displaySizeWidth);
	_displayVSync = GetB("displayVSync", _displayVSync);
	_gapi = GetI("gapi", _gapi);
	_gpuAnisotropyLevel = GetI("gpuAnisotropyLevel", _gpuAnisotropyLevel);
	_gpuAntiAliasingLevel = GetI("gpuAntiAliasingLevel", _gpuAntiAliasingLevel);
	_gpuShaderQuality = static_cast<Quality>(GetI("gpuShaderQuality", +_gpuShaderQuality));
	_gpuTessellation = GetB("gpuTessellation", _gpuTessellation);
	_gpuTextureLodLevel = GetI("gpuTextureLodLevel", _gpuTextureLodLevel);
	_gpuTrilinearFilter = GetB("gpuTrilinearFilter", _gpuTrilinearFilter);
	_inputMouseSensitivity = GetF("inputMouseSensitivity", _inputMouseSensitivity);
	_openXR = GetB("openXR", _openXR);
	_postProcessAntiAliasing = GetB("postProcessAntiAliasing", _postProcessAntiAliasing);
	_postProcessBloom = GetB("postProcessBloom", _postProcessBloom);
	_postProcessCinema = GetB("postProcessCinema", _postProcessCinema);
	_postProcessLightShafts = GetB("postProcessLightShafts", _postProcessLightShafts);
	_postProcessMotionBlur = GetB("postProcessMotionBlur", _postProcessMotionBlur);
	_postProcessSSAO = GetB("postProcessSSAO", _postProcessSSAO);
	_postProcessSSR = GetB("postProcessSSR", _postProcessSSR);
	_sceneAmbientOcclusion = GetB("sceneAmbientOcclusion", _sceneAmbientOcclusion);
	_sceneGrassDensity = GetI("sceneGrassDensity", _sceneGrassDensity);
	_sceneShadowQuality = static_cast<Quality>(GetI("sceneShadowQuality", +_sceneShadowQuality));
	_sceneWaterQuality = static_cast<WaterQuality>(GetI("sceneWaterQuality", +_sceneWaterQuality));
	_uiLang = GetS("uiLang", _C(_uiLang));
	_xrFOV = GetF("xrFOV", _xrFOV);
	_xrHeight = GetF("xrHeight", _xrHeight);
}

void Settings::HandleHighDpi()
{
	if (_displayAllowHighDPI)
	{
		if (!SetProcessDPIAware())
			throw VERUS_RUNTIME_ERROR << "SetProcessDPIAware()";
		UpdateHighDpiScale();
	}
}

void Settings::HandleCommandLineArgs()
{
	switch (_commandLine._gapi)
	{
	case 0:  _gapi = 0; break;
	case 11: _gapi = 11; break;
	case 12: _gapi = 12; break;
	}

	switch (_commandLine._openXR)
	{
	case 0: _openXR = false; break;
	case 1: _openXR = true; break;
	}

	if (_commandLine._exclusiveFullscreen)
	{
		_displayMode = DisplayMode::exclusiveFullscreen;
		MatchScreen();
	}
	if (_commandLine._windowed)
	{
		_displayMode = DisplayMode::windowed;
		_displaySizeWidth = 1280;
		_displaySizeHeight = 720;
	}
	if (_commandLine._borderlessWindowed)
	{
		_displayMode = DisplayMode::borderlessWindowed;
		MatchScreen();
	}

	if (_commandLine._xrHeight != -FLT_MAX)
		_xrHeight = _commandLine._xrHeight;
}

void Settings::Validate()
{
	if (!_info._appName)
		_info._appName = "Unnamed";
	if (!_info._appVersion)
		_info._appVersion = VERUS_MAKE_VERSION(0, 1, 0);
	if (!_info._engineName)
		_info._engineName = VERUS_ENGINE_NAME;
	if (!_info._engineVersion)
		_info._engineVersion = VERUS_SDK_VERSION;

	_displayFOV = Math::Clamp<float>(_displayFOV, 60, 90);
	_displayOffscreenScale = Math::Clamp(_displayOffscreenScale, 0.25f, 4.f);
	_displaySizeWidth = Math::Clamp(_displaySizeWidth, 480, 0x2000);
	_displaySizeHeight = Math::Clamp(_displaySizeHeight, 270, 0x2000);
	_gpuAnisotropyLevel = Math::Clamp(_gpuAnisotropyLevel, 0, 16);
	_gpuAntiAliasingLevel = Math::Clamp(_gpuAntiAliasingLevel, 0, 16);
	_gpuShaderQuality = Math::Clamp(_gpuShaderQuality, Quality::low, Quality::ultra);
	_gpuTextureLodLevel = Math::Clamp(_gpuTextureLodLevel, 0, 4);
	_sceneGrassDensity = Math::Clamp(_sceneGrassDensity, 100, 1000);
	_sceneShadowQuality = Math::Clamp(_sceneShadowQuality, Quality::low, Quality::ultra);
	_sceneWaterQuality = Math::Clamp(_sceneWaterQuality, WaterQuality::solidColor, WaterQuality::trueWavesRefraction);
	_xrFOV = Math::Clamp<float>(_xrFOV, 45, 135);
	_xrHeight = Math::Clamp<float>(_xrHeight, -3, 3);

	if (_uiLang != "RU" && _uiLang != "EN")
		_uiLang = "EN";
}

void Settings::Save()
{
	Clear();

	Set("displayAllowHighDPI", _displayAllowHighDPI);
	Set("displayFOV", _displayFOV);
	Set("displayMode", +_displayMode);
	Set("displayOffscreenDraw", _displayOffscreenDraw);
	Set("displayOffscreenScale", _displayOffscreenScale);
	Set("displaySizeHeight", _displaySizeHeight);
	Set("displaySizeWidth", _displaySizeWidth);
	Set("displayVSync", _displayVSync);
	Set("gapi", _gapi);
	Set("gpuAnisotropyLevel", _gpuAnisotropyLevel);
	Set("gpuAntiAliasingLevel", _gpuAntiAliasingLevel);
	Set("gpuShaderQuality", +_gpuShaderQuality);
	Set("gpuTessellation", _gpuTessellation);
	Set("gpuTextureLodLevel", _gpuTextureLodLevel);
	Set("gpuTrilinearFilter", _gpuTrilinearFilter);
	Set("inputMouseSensitivity", _inputMouseSensitivity);
	Set("openXR", _openXR);
	Set("postProcessAntiAliasing", _postProcessAntiAliasing);
	Set("postProcessBloom", _postProcessBloom);
	Set("postProcessCinema", _postProcessCinema);
	Set("postProcessLightShafts", _postProcessLightShafts);
	Set("postProcessMotionBlur", _postProcessMotionBlur);
	Set("postProcessSSAO", _postProcessSSAO);
	Set("postProcessSSR", _postProcessSSR);
	Set("sceneAmbientOcclusion", _sceneAmbientOcclusion);
	Set("sceneGrassDensity", _sceneGrassDensity);
	Set("sceneShadowQuality", +_sceneShadowQuality);
	Set("sceneWaterQuality", +_sceneWaterQuality);
	Set("uiLang", _C(_uiLang));
	Set("xrFOV", _xrFOV);
	Set("xrHeight", _xrHeight);

	Json::Save();
}

void Settings::MatchScreen()
{
#ifdef _WIN32
	_displaySizeWidth = GetSystemMetrics(SM_CXSCREEN);
	_displaySizeHeight = GetSystemMetrics(SM_CYSCREEN);
#else
	_displaySizeWidth = 1920;
	_displaySizeHeight = 1080;
#endif
}

void Settings::LoadLocalizedStrings(CSZ url)
{
	Vector<BYTE> vData;
	IO::FileSystem::LoadResource(url, vData, IO::FileSystem::LoadDesc(true));

	pugi::xml_document doc;
	const pugi::xml_parse_result result = doc.load_buffer_inplace(vData.data(), vData.size());
	if (!result)
		throw VERUS_RECOVERABLE << "load_buffer_inplace(); " << result.description();
	pugi::xml_node root = doc.first_child();

	_mapLocalizedStrings.clear();
	for (const auto node : root.children("s"))
	{
		const auto attrKey = node.attribute("id");
		const auto attrVal = node.attribute(_C(_uiLang));
		if (attrKey && attrVal)
			_mapLocalizedStrings[attrKey.value()] = attrVal.value();
	}
}

CSZ Settings::GetLocalizedString(CSZ id) const
{
	VERUS_IF_FOUND_IN(TMapLocalizedStrings, _mapLocalizedStrings, id, it)
		return _C(it->second);
	return id;
}

void Settings::UpdateHighDpiScale()
{
	float ddpi = 0;
	float hdpi = 0;
	float vdpi = 0;
	if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) < 0)
	{
		VERUS_LOG_INFO("SDL_GetDisplayDPI(); " << SDL_GetError());
		_highDpiScale = 1;
	}
	else
	{
		_highDpiScale = vdpi / 96;
	}
}

int Settings::GetFontSize() const
{
	if (_displayAllowHighDPI)
		return static_cast<int>(15 * _highDpiScale);
	return 15;
}

int Settings::Scale(int size, float extraScale) const
{
	return _displayOffscreenDraw ? static_cast<int>(size * _displayOffscreenScale * extraScale + 0.5f) : size;
}

float Settings::GetScale() const
{
	return _displayOffscreenDraw ? _displayOffscreenScale : 1;
}
