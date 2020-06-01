#include "verus.h"

using namespace verus;
using namespace verus::App;

Settings::Settings()
{
	VERUS_ZERO_MEM(_commandLine);
#ifdef _WIN32
	if (0x419 == GetUserDefaultUILanguage())
		_uiLang = "RU";
#endif
	SetQuality(Quality::medium);
}

Settings::~Settings()
{
}

void Settings::Load()
{
	Json::Load();

	_quality = static_cast<Quality>(GetI("quality", +_quality));
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
	_screenAllowHighDPI = GetB("screenAllowHighDPI", _screenAllowHighDPI);
	_screenFOV = GetF("screenFOV", _screenFOV);
	_screenOffscreenDraw = GetF("screenOffscreenDraw", _screenOffscreenDraw);
	_screenSizeHeight = GetI("screenSizeHeight", _screenSizeHeight);
	_screenSizeWidth = GetI("screenSizeWidth", _screenSizeWidth);
	_screenVSync = GetB("screenVSync", _screenVSync);
	_screenWindowed = GetB("screenWindowed", _screenWindowed);
	_uiLang = GetS("uiLang", _C(_uiLang));
}

void Settings::Validate()
{
	_quality = Math::Clamp(_quality, Quality::low, Quality::ultra);
	_gpuAnisotropyLevel = Math::Clamp(_gpuAnisotropyLevel, 0, 16);
	_gpuAntialiasingLevel = Math::Clamp(_gpuAntialiasingLevel, 0, 16);
	_gpuTextureLodLevel = Math::Clamp(_gpuTextureLodLevel, 0, 4);
	_sceneGrassDensity = Math::Clamp(_sceneGrassDensity, 15, 1500);
	_sceneShadowQuality = Math::Clamp(_sceneShadowQuality, ShadowQuality::none, ShadowQuality::ultra);
	_sceneWaterQuality = Math::Clamp(_sceneWaterQuality, WaterQuality::solidColor, WaterQuality::trueWavesRefraction);
	_screenFOV = Math::Clamp<float>(_screenFOV, 60, 90);
	_screenSizeHeight = Math::Clamp(_screenSizeHeight, 270, 0x2000);
	_screenSizeWidth = Math::Clamp(_screenSizeWidth, 480, 0x2000);

	if (_uiLang != "RU" && _uiLang != "EN")
		_uiLang = "EN";
}

void Settings::Save()
{
	Set("quality", +_quality);
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
	Set("screenAllowHighDPI", _screenAllowHighDPI);
	Set("screenFOV", _screenFOV);
	Set("screenOffscreenDraw", _screenOffscreenDraw);
	Set("screenSizeHeight", _screenSizeHeight);
	Set("screenSizeWidth", _screenSizeWidth);
	Set("screenVSync", _screenVSync);
	Set("screenWindowed", _screenWindowed);
	Set("uiLang", _C(_uiLang));

	Json::Save();
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
		_commandLine._normalMapOnlyLS = _commandLine._normalMapOnlyLS || IsArg(i, "--normal-map-only-ls");
		_commandLine._physicsDebug = _commandLine._physicsDebug || IsArg(i, "--physics-debug");
		_commandLine._physicsDebugFull = _commandLine._physicsDebugFull || IsArg(i, "--physics-debug-full");
		_commandLine._profiler = _commandLine._profiler || IsArg(i, "--profiler");
		_commandLine._testTexToPix = _commandLine._testTexToPix || IsArg(i, "--test-tex-to-pix");
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
		_sceneGrassDensity = 500;
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
		_sceneGrassDensity = 1500;
		_sceneShadowQuality = ShadowQuality::cascaded;
		_sceneWaterQuality = WaterQuality::trueWavesReflection;
		break;
	case Quality::ultra:
		_gpuAnisotropyLevel = 16;
		_postProcessBloom = true;
		_postProcessCinema = true;
		_postProcessMotionBlur = true;
		_postProcessSSAO = true;
		_sceneGrassDensity = 1500;
		_sceneShadowQuality = ShadowQuality::ultra;
		_sceneWaterQuality = WaterQuality::trueWavesRefraction;
		_screenSizeHeight = 1080;
		_screenSizeWidth = 1980;
		break;
	}
}

void Settings::MatchScreen()
{
	SDL_DisplayMode dm;
	if (SDL_GetCurrentDisplayMode(0, &dm) < 0)
		throw VERUS_RUNTIME_ERROR << "SDL_GetCurrentDisplayMode(), " << SDL_GetError();
	_screenSizeWidth = dm.w;
	_screenSizeHeight = dm.h;
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
	if (_screenAllowHighDPI)
		return static_cast<int>(15 * _highDpiScale);
	return 15;
}
