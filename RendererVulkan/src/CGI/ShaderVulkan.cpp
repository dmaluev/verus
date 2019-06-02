#include "stdafx.h"

using namespace verus;
using namespace verus::CGI;

struct ShaderInclude : BaseShaderInclude
{
	virtual void Open(CSZ filename, void** ppData, UINT32* pBytes) override
	{
		const String url = String("[Shaders]:") + filename;
		Vector<BYTE> vData;
		IO::FileSystem::LoadResource(_C(url), vData);
		char* p = new char[vData.size()];
		memcpy(p, vData.data(), vData.size());
		*pBytes = Utils::Cast32(vData.size());
		*ppData = p;
	}

	virtual void Close(void* pData) override
	{
		delete[] pData;
	}
};

// ShaderVulkan:

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

	ShaderInclude inc;
#ifdef _DEBUG
	const UINT32 flags = 1;
#else
	const UINT32 flags = 0;
#endif
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
		if (!pCode || !size)
			return;
		VERUS_QREF_RENDERER_VULKAN;
		VkResult res = VK_SUCCESS;
		VkShaderModuleCreateInfo vksmci = {};
		vksmci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vksmci.codeSize = size;
		vksmci.pCode = pCode;
		if (VK_SUCCESS != (res = vkCreateShaderModule(pRendererVulkan->GetVkDevice(), &vksmci, pRendererVulkan->GetAllocator(), &shaderModule)))
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

		if (strstr(source, " mainVS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_VS";
			if (!RendererVulkan::VerusCompile(source, vDefines.data(), &inc, _C(entryVS), "vs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::vs]);
		}

		if (strstr(source, " mainHS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_HS";
			if (!RendererVulkan::VerusCompile(source, vDefines.data(), &inc, _C(entryHS), "hs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::hs]);
		}

		if (strstr(source, " mainDS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_DS";
			if (!RendererVulkan::VerusCompile(source, vDefines.data(), &inc, _C(entryDS), "ds", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::ds]);
		}

		if (strstr(source, " mainGS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_GS";
			if (!RendererVulkan::VerusCompile(source, vDefines.data(), &inc, _C(entryGS), "gs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::gs]);
		}

		if (strstr(source, " mainFS("))
		{
			compiled._numStages++;
			vDefines[typeIndex] = "_FS";
			if (!RendererVulkan::VerusCompile(source, vDefines.data(), &inc, _C(entryFS), "fs", flags, &pCode, &size, &pErrorMsgs))
				CheckErrorMsgs(pErrorMsgs);
			CreateShaderModule(pCode, size, compiled._shaderModules[+Stage::fs]);
		}

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
