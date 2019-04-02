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

void Settings::LoadValidateSave()
{
	Load();
	Validate();
	Save();
}

void Settings::Load()
{
	Json::Load();

	_quality = static_cast<Quality>(GetI("quality", +_quality));
	_gapi = GetI("gapi", _gapi);
	_gpuAnisotropyLevel = GetI("gpuAnisotropyLevel", _gpuAnisotropyLevel);
	_gpuAntialiasingLevel = GetI("gpuAntialiasingLevel", _gpuAntialiasingLevel);
	_gpuDepthTexture = GetI("gpuDepthTexture", _gpuDepthTexture);
	_gpuForcedProfile = GetS("gpuForcedProfile", _C(_gpuForcedProfile));
	_gpuOffscreenRender = GetB("gpuOffscreenRender", _gpuOffscreenRender);
	_gpuParallaxMapping = GetB("gpuParallaxMapping", _gpuParallaxMapping);
	_gpuPerPixelLighting = GetB("gpuPerPixelLighting", _gpuPerPixelLighting);
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
	_screenFOV = GetF("screenFOV", _screenFOV);
	_screenSizeHeight = GetI("screenSizeHeight", _screenSizeHeight);
	_screenSizeWidth = GetI("screenSizeWidth", _screenSizeWidth);
	_screenVSync = GetB("screenVSync", _screenVSync);
	_screenWindowed = GetB("screenWindowed", _screenWindowed);
	_uiLang = GetS("uiLang", _C(_uiLang));
}

void Settings::Save()
{
	Set("quality", +_quality);
	Set("gapi", _gapi);
	Set("gpuAnisotropyLevel", _gpuAnisotropyLevel);
	Set("gpuAntialiasingLevel", _gpuAntialiasingLevel);
	Set("gpuDepthTexture", _gpuDepthTexture);
	Set("gpuForcedProfile", _C(_gpuForcedProfile));
	Set("gpuOffscreenRender", _gpuOffscreenRender);
	Set("gpuParallaxMapping", _gpuParallaxMapping);
	Set("gpuPerPixelLighting", _gpuPerPixelLighting);
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
	Set("screenFOV", _screenFOV);
	Set("screenSizeHeight", _screenSizeHeight);
	Set("screenSizeWidth", _screenSizeWidth);
	Set("screenVSync", _screenVSync);
	Set("screenWindowed", _screenWindowed);
	Set("uiLang", _C(_uiLang));

	Json::Save();
}

void Settings::Validate()
{
	_quality = Math::Clamp(_quality, Quality::low, Quality::ultra);
	_gpuAnisotropyLevel = Math::Clamp(_gpuAnisotropyLevel, 0, 16);
	_gpuAntialiasingLevel = Math::Clamp(_gpuAntialiasingLevel, 0, 16);
	_gpuTextureLodLevel = Math::Clamp(_gpuTextureLodLevel, 0, 4);
	_screenFOV = Math::Clamp<float>(_screenFOV, 60, 90);
	_screenSizeHeight = Math::Clamp(_screenSizeHeight, 270, 0x2000);
	_screenSizeWidth = Math::Clamp(_screenSizeWidth, 480, 0x2000);

	if (_gpuForcedProfile == "sm_2_0")
	{
		_postProcessMotionBlur = false;
		_postProcessSSAO = false;
		_sceneShadowQuality = Math::Min(_sceneShadowQuality, ShadowQuality::filtered);
	}

	if (!_gpuPerPixelLighting)
	{
		_gpuParallaxMapping = false;
		_sceneWaterQuality = Math::Clamp(_sceneWaterQuality, WaterQuality::solidColor, WaterQuality::distortedReflection);
	}
	_postProcessBloom = _postProcessBloom && _gpuOffscreenRender;
	_sceneGrassDensity = Math::Clamp(_sceneGrassDensity, 15, 1500);
	_sceneShadowQuality = Math::Clamp(_sceneShadowQuality, ShadowQuality::none, ShadowQuality::cascaded);
	_sceneWaterQuality = Math::Clamp(_sceneWaterQuality, WaterQuality::solidColor, WaterQuality::trueWavesRefraction);

	if (_sceneShadowQuality >= ShadowQuality::cascaded)
		_gpuDepthTexture = 1;

	if (_uiLang != "RU" && _uiLang != "EN")
		_uiLang = "EN";
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
		_commandLine.normalMapOnlyLS = _commandLine.normalMapOnlyLS || IsArg(i, "--normal-map-only-ls");
		_commandLine.physicsDebug = _commandLine.physicsDebug || IsArg(i, "--physics-debug");
		_commandLine.physicsDebugFull = _commandLine.physicsDebugFull || IsArg(i, "--physics-debug-full");
		_commandLine.profiler = _commandLine.profiler || IsArg(i, "--profiler");
		_commandLine.testTexToPix = _commandLine.testTexToPix || IsArg(i, "--test-tex-to-pix");
	}

	VERUS_QREF_UTILS;
	//utils.SetupPathsEx(); // Only now we can finally do this.

	SetFilename("Settings.json");
	const String pathName = _C(GetFilename());

	VERUS_FOR(i, argc)
	{
		if (IsArg(i, "--q-low"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathName));
			SetQuality(Quality::low);
		}
		if (IsArg(i, "--q-medium"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathName));
			SetQuality(Quality::medium);
		}
		if (IsArg(i, "--q-high"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathName));
			SetQuality(Quality::high);
		}
		if (IsArg(i, "--q-ultra"))
		{
			const bool ret = IO::FileSystem::Delete(_C(pathName));
			SetQuality(Quality::ultra);
		}
	}
}

void Settings::SetQuality(Quality q)
{
	_quality = q;
	_gpuAnisotropyLevel = 0;
	_gpuAntialiasingLevel = 0;
	_gpuDepthTexture = 0;
	_gpuForcedProfile = "";
	_gpuOffscreenRender = true;
	_gpuParallaxMapping = false;
	_gpuPerPixelLighting = true;
	_gpuTextureLodLevel = 0;
	_gpuTrilinearFilter = false;
	_postProcessBloom = false;
	_postProcessCinema = false;
	_postProcessMotionBlur = false;
	_postProcessSSAO = false;
	_sceneGrassDensity = 1000;
	_sceneShadowQuality = ShadowQuality::sharp;
	_sceneWaterQuality = WaterQuality::distortedReflection;
	_screenVSync = false;
	_screenWindowed = true;

	switch (q)
	{
	case Quality::low:
		_gpuDepthTexture = -1;
		_gpuForcedProfile = "sm_3_0";
		_sceneGrassDensity = 500;
		_sceneShadowQuality = ShadowQuality::none;
		_sceneWaterQuality = WaterQuality::solidColor;
		_screenSizeHeight = 600;
		_screenSizeWidth = 800;
		break;
	case Quality::medium:
		_gpuAnisotropyLevel = 4;
		_gpuForcedProfile = "sm_3_0";
		_screenSizeHeight = 768;
		_screenSizeWidth = 1024;
		break;
	case Quality::high:
		_gpuAnisotropyLevel = 8;
		_gpuTrilinearFilter = true;
		_postProcessBloom = true;
		_postProcessCinema = true;
		_postProcessSSAO = true;
		_sceneGrassDensity = 1500;
		_sceneShadowQuality = ShadowQuality::filtered;
		_sceneWaterQuality = WaterQuality::trueWavesReflection;
		_screenSizeHeight = 720;
		_screenSizeWidth = 1280;
		break;
	case Quality::ultra:
		_gpuAnisotropyLevel = 16;
		_gpuDepthTexture = 1;
		_gpuTrilinearFilter = true;
		_postProcessBloom = true;
		_postProcessCinema = true;
		_postProcessMotionBlur = true;
		_postProcessSSAO = true;
		_sceneGrassDensity = 1500;
		_sceneShadowQuality = ShadowQuality::cascaded;
		_sceneWaterQuality = WaterQuality::trueWavesRefraction;
		_screenSizeHeight = 1080;
		_screenSizeWidth = 1980;
		break;
	}

#ifdef _WIN32
#	ifndef _DEBUG
	_screenSizeHeight = GetSystemMetrics(SM_CYSCREEN);
	_screenSizeWidth = GetSystemMetrics(SM_CXSCREEN);
	_screenWindowed = false;
#	endif
#else // Linux?
	_gapi = 0;
	_gpuForcedProfile = "arb1";
#endif
}

void Settings::MatchScreen()
{
	SDL_DisplayMode dm;
	if (SDL_GetCurrentDisplayMode(0, &dm) < 0)
		throw VERUS_RUNTIME_ERROR << "SDL_GetCurrentDisplayMode(), " << SDL_GetError();
	_screenSizeWidth = dm.w;
	_screenSizeHeight = dm.h;
}
