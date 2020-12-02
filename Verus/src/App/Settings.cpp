// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::App;

Settings::Settings()
{
#ifdef _WIN32
	if (0x419 == GetUserDefaultUILanguage())
		_uiLang = "RU";
#endif
	SetQuality(Quality::medium);
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
		if (IsArg(i, "--efs") || IsArg(i, "--fullscreen"))
			_commandLine._exclusiveFullscreen = true;
		if (IsArg(i, "--wnd") || IsArg(i, "--windowed"))
			_commandLine._windowed = true;
		if (IsArg(i, "--bwnd"))
			_commandLine._borderlessWindowed = true;
	}

	SetFilename("Settings.json");
	const String pathname = _C(GetFilename());

	VERUS_FOR(i, argc)
	{
		if (IsArg(i, "--q-low"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(Quality::low);
		}
		if (IsArg(i, "--q-medium"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(Quality::medium);
		}
		if (IsArg(i, "--q-high"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(Quality::high);
		}
		if (IsArg(i, "--q-ultra"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathname));
			SetQuality(Quality::ultra);
		}
	}
}

void Settings::SetQuality(Quality q)
{
	_quality = q;

	switch (q)
	{
	case Quality::low:
		_gpuAnisotropyLevel = 0;
		_gpuTrilinearFilter = false;
		_sceneShadowQuality = ShadowQuality::linear;
		_sceneWaterQuality = WaterQuality::solidColor;
		break;
	case Quality::medium:
		break;
	case Quality::high:
		_gpuAnisotropyLevel = 8;
		_postProcessBloom = true;
		_postProcessCinema = true;
		_postProcessSSAO = true;
		_sceneGrassDensity = 1000;
		_sceneShadowQuality = ShadowQuality::cascaded;
		_sceneWaterQuality = WaterQuality::trueWavesReflection;
		break;
	case Quality::ultra:
		_gpuAnisotropyLevel = 16;
		_postProcessBloom = true;
		_postProcessCinema = true;
		_postProcessMotionBlur = true;
		_postProcessSSAO = true;
		_sceneGrassDensity = 1000;
		_sceneShadowQuality = ShadowQuality::ultra;
		_sceneWaterQuality = WaterQuality::trueWavesRefraction;
		_displaySizeHeight = 1080;
		_displaySizeWidth = 1980;
		break;
	}
}

void Settings::Load()
{
	Json::Load();

	_quality = static_cast<Quality>(GetI("quality", +_quality));
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
	_gpuAntialiasingLevel = GetI("gpuAntialiasingLevel", _gpuAntialiasingLevel);
	_gpuTessellation = GetB("gpuTessellation", _gpuTessellation);
	_gpuTextureLodLevel = GetI("gpuTextureLodLevel", _gpuTextureLodLevel);
	_gpuTrilinearFilter = GetB("gpuTrilinearFilter", _gpuTrilinearFilter);
	_inputMouseSensitivity = GetF("inputMouseSensitivity", _inputMouseSensitivity);
	_postProcessBloom = GetB("postProcessBloom", _postProcessBloom);
	_postProcessCinema = GetB("postProcessCinema", _postProcessCinema);
	_postProcessMotionBlur = GetB("postProcessMotionBlur", _postProcessMotionBlur);
	_postProcessSSAO = GetB("postProcessSSAO", _postProcessSSAO);
	_sceneGrassDensity = GetI("sceneGrassDensity", _sceneGrassDensity);
	_sceneShadowQuality = static_cast<ShadowQuality>(GetI("sceneShadowQuality", +_sceneShadowQuality));
	_sceneWaterQuality = static_cast<WaterQuality>(GetI("sceneWaterQuality", +_sceneWaterQuality));
	_uiLang = GetS("uiLang", _C(_uiLang));
}

void Settings::HandleHighDpi()
{
	if (_displayAllowHighDPI)
	{
		HRESULT hr = 0;
		if (FAILED(hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE)))
			throw VERUS_RUNTIME_ERROR << "SetProcessDpiAwareness(), hr=" << VERUS_HR(hr);
		UpdateHighDpiScale();
	}
}

void Settings::HandleCommandLineArgs()
{
	switch (_commandLine._gapi)
	{
	case 0: _gapi = 0; break;
	case 12: _gapi = 12; break;
	}

	if (_commandLine._exclusiveFullscreen)
	{
		_displayMode = DisplayMode::exclusiveFullscreen;
		MatchScreen();
	}
	if (_commandLine._windowed)
	{
		_displayMode = DisplayMode::windowed;
		_displaySizeHeight = 720;
		_displaySizeWidth = 1280;
	}
	if (_commandLine._borderlessWindowed)
	{
		_displayMode = DisplayMode::borderlessWindowed;
		MatchScreen();
	}
}

void Settings::Validate()
{
	_quality = Math::Clamp(_quality, Quality::low, Quality::ultra);
	_displayFOV = Math::Clamp<float>(_displayFOV, 60, 90);
	_displaySizeHeight = Math::Clamp(_displaySizeHeight, 270, 0x2000);
	_displaySizeWidth = Math::Clamp(_displaySizeWidth, 480, 0x2000);
	_gpuAnisotropyLevel = Math::Clamp(_gpuAnisotropyLevel, 0, 16);
	_gpuAntialiasingLevel = Math::Clamp(_gpuAntialiasingLevel, 0, 16);
	_gpuTextureLodLevel = Math::Clamp(_gpuTextureLodLevel, 0, 4);
	_sceneGrassDensity = Math::Clamp(_sceneGrassDensity, 100, 1000);
	_sceneShadowQuality = Math::Clamp(_sceneShadowQuality, ShadowQuality::none, ShadowQuality::ultra);
	_sceneWaterQuality = Math::Clamp(_sceneWaterQuality, WaterQuality::solidColor, WaterQuality::trueWavesRefraction);

	if (_uiLang != "RU" && _uiLang != "EN")
		_uiLang = "EN";
}

void Settings::Save()
{
	Clear();

	Set("quality", +_quality);
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
	Set("gpuAntialiasingLevel", _gpuAntialiasingLevel);
	Set("gpuTessellation", _gpuTessellation);
	Set("gpuTextureLodLevel", _gpuTextureLodLevel);
	Set("gpuTrilinearFilter", _gpuTrilinearFilter);
	Set("inputMouseSensitivity", _inputMouseSensitivity);
	Set("postProcessBloom", _postProcessBloom);
	Set("postProcessCinema", _postProcessCinema);
	Set("postProcessMotionBlur", _postProcessMotionBlur);
	Set("postProcessSSAO", _postProcessSSAO);
	Set("sceneGrassDensity", _sceneGrassDensity);
	Set("sceneShadowQuality", +_sceneShadowQuality);
	Set("sceneWaterQuality", +_sceneWaterQuality);
	Set("uiLang", _C(_uiLang));

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
		throw VERUS_RECOVERABLE << "load_buffer_inplace(), " << result.description();
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
		throw VERUS_RUNTIME_ERROR << "SDL_GetDisplayDPI(), " << SDL_GetError();
	_highDpiScale = vdpi / 96;
}

int Settings::GetFontSize() const
{
	if (_displayAllowHighDPI)
		return static_cast<int>(15 * _highDpiScale);
	return 15;
}
