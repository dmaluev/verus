#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

ShaderVulkan::ShaderVulkan()
{
}

ShaderVulkan::~ShaderVulkan()
{
	Done();
}

void ShaderVulkan::Init(CSZ source, CSZ* branches)
{
	VERUS_INIT();
	VERUS_QREF_CONST_SETTINGS;

	UINT32* pCode = nullptr;
	UINT32 size = 0;
	CSZ pErrorMsgs = nullptr;

	auto CheckErrorMsgs = [this](CSZ pErrorMsgs)
	{
		if (pErrorMsgs)
			OnError(pErrorMsgs);
	};

	auto CreateShaderModule = [](const UINT32* pCode, UINT32 size, VkShaderModule& shaderModule)
	{
		VERUS_QREF_RENDERER_VULKAN;
		VkResult res = VK_SUCCESS;
		VkShaderModuleCreateInfo vksmci = {};
		vksmci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vksmci.codeSize = size;
		vksmci.pCode = pCode;
		if (VK_SUCCESS != (res = vkCreateShaderModule(pRendererVulkan->GetDevice(), &vksmci, pRendererVulkan->GetAllocator(), &shaderModule)))
			throw VERUS_RUNTIME_ERROR << "vkCreateShaderModule(), res=" << res;
	};

	while (*branches)
	{
		String entryVS, entryHS, entryDS, entryGS, entryFS;
		Vector<String> vMacroName;
		Vector<String> vMacroValue;
		const String entry = Parse(*branches, entryVS, entryHS, entryDS, entryGS, entryFS, vMacroName, vMacroValue, "DEF_");

		if (IsInIgnoreList(_C(entry)))
		{
			branches++;
			continue;
		}

		// <User defines>
		Vector<CSZ> vDefines;
		vDefines.reserve(40);
		const int num = Utils::Cast32(vMacroName.size());
		VERUS_FOR(i, num)
		{
			vDefines.push_back(_C(vMacroName[i]));
			vDefines.push_back(_C(vMacroValue[i]));
		}
		// </User defines>

		// <System defines>
		char defAnisotropyLevel[64] = {};
		{
			sprintf_s(defAnisotropyLevel, "%d", settings._gpuAnisotropyLevel);
			vDefines.push_back("_ANISOTROPY_LEVEL");
			vDefines.push_back(defAnisotropyLevel);
		}
		char defShadowQuality[64] = {};
		{
			sprintf_s(defShadowQuality, "%d", settings._sceneShadowQuality);
			vDefines.push_back("_SHADOW_QUALITY");
			vDefines.push_back(defShadowQuality);
		}
		char defWaterQuality[64] = {};
		{
			sprintf_s(defWaterQuality, "%d", settings._sceneWaterQuality);
			vDefines.push_back("_WATER_QUALITY");
			vDefines.push_back(defWaterQuality);
		}
		vDefines.push_back("_VULKAN");
		vDefines.push_back("1");
		const int typeIndex = Utils::Cast32(vDefines.size());
		vDefines.push_back("_XS");
		vDefines.push_back("1");
		vDefines.push_back(nullptr);
		// </System defines>

		Compiled compiled;

		vDefines[typeIndex] = "_VS";
		if (!RendererVulkan::VerusCompile(source, vDefines.data(), _C(entryVS), "vs", 0, &pCode, &size, &pErrorMsgs))
			CheckErrorMsgs(pErrorMsgs);
		CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::vs]);

		vDefines[typeIndex] = "_HS";
		if (!RendererVulkan::VerusCompile(source, vDefines.data(), _C(entryHS), "hs", 0, &pCode, &size, &pErrorMsgs))
			CheckErrorMsgs(pErrorMsgs);
		CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::hs]);

		vDefines[typeIndex] = "_DS";
		if (!RendererVulkan::VerusCompile(source, vDefines.data(), _C(entryDS), "ds", 0, &pCode, &size, &pErrorMsgs))
			CheckErrorMsgs(pErrorMsgs);
		CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::ds]);

		vDefines[typeIndex] = "_GS";
		if (!RendererVulkan::VerusCompile(source, vDefines.data(), _C(entryGS), "gs", 0, &pCode, &size, &pErrorMsgs))
			CheckErrorMsgs(pErrorMsgs);
		CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::gs]);

		vDefines[typeIndex] = "_FS";
		if (!RendererVulkan::VerusCompile(source, vDefines.data(), _C(entryFS), "fs", 0, &pCode, &size, &pErrorMsgs))
			CheckErrorMsgs(pErrorMsgs);
		CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::fs]);

		_mapCompiled[entry] = compiled;

		branches++;
	}
}

void ShaderVulkan::Done()
{
	VERUS_DONE(ShaderVulkan);
}

void ShaderVulkan::OnError(CSZ s)
{
	VERUS_QREF_RENDERER;
	if (strstr(s, ": error "))
		renderer.OnShaderError(s);
	else
		renderer.OnShaderWarning(s);
}
