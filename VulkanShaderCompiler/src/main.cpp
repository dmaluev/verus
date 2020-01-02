#include "stdafx.h"

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

typedef const char* CSZ;

struct ShaderInclude
{
public:
	virtual void Open(CSZ filename, void** ppData, UINT32* pBytes) = 0;
	virtual void Close(void* pData) = 0;
};

bool                                   g_init = false;
thread_local ShaderInclude* g_pShaderInclude = nullptr;
thread_local std::string               g_ret;
thread_local std::vector<unsigned int> g_vSpirv;

class MyIncluder : public glslang::TShader::Includer
{
public:
	MyIncluder() {}
	virtual ~MyIncluder() override {}

	virtual IncludeResult* includeSystem(
		CSZ headerName,
		CSZ includerName,
		size_t inclusionDepth) override
	{
		void* pData = nullptr;
		UINT32 size = 0;
		g_pShaderInclude->Open(headerName, &pData, &size);
		return new IncludeResult(headerName, static_cast<CSZ>(pData), size, pData);
	}

	virtual IncludeResult* includeLocal(
		CSZ headerName,
		CSZ includerName,
		size_t inclusionDepth) override
	{
		void* pData = nullptr;
		UINT32 size = 0;
		g_pShaderInclude->Open(headerName, &pData, &size);
		return new IncludeResult(headerName, static_cast<CSZ>(pData), size, pData);
	}

	virtual void releaseInclude(IncludeResult* result) override
	{
		if (result)
		{
			g_pShaderInclude->Close(result->userData);
			delete result;
		}
	}
};

extern "C"
{
	VERUS_DLL_EXPORT void VulkanCompilerInit()
	{
		if (!g_init)
		{
			g_init = true;
			glslang::InitializeProcess();
		}
	}

	VERUS_DLL_EXPORT void VulkanCompilerDone()
	{
		if (g_init)
		{
			g_init = false;
			glslang::FinalizeProcess();
		}
	}

	VERUS_DLL_EXPORT bool VulkanCompile(CSZ source, CSZ sourceName, CSZ* defines, ShaderInclude* pInclude,
		CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs)
	{
		if (!g_init)
		{
			g_ret = "Call InitializeProcess";
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}

		struct RAII
		{
			~RAII()
			{
				g_pShaderInclude = nullptr;
			}
		} raii;

		g_pShaderInclude = pInclude;
		*ppCode = nullptr;
		*pSize = 0;

		g_ret.clear();
		g_vSpirv.clear();
		std::stringstream ss;

		EShLanguage stage = EShLangCount;
		if (!strcmp(target, "vs"))
			stage = EShLangVertex;
		else if (!strcmp(target, "hs"))
			stage = EShLangTessControl;
		else if (!strcmp(target, "ds"))
			stage = EShLangTessEvaluation;
		else if (!strcmp(target, "gs"))
			stage = EShLangGeometry;
		else if (!strcmp(target, "fs"))
			stage = EShLangFragment;
		else if (!strcmp(target, "cs"))
			stage = EShLangCompute;

		std::stringstream ssDefines;
		if (defines)
		{
			while (*defines)
			{
				ssDefines << "#define ";
				ssDefines << *defines++;
				ssDefines << " ";
				ssDefines << *defines++;
				ssDefines << "\r\n";
			}
		}
		const std::string preamble = ssDefines.str();

		const int clientInputSemanticsVersion = 100;
		const int defaultVersion = 110;

		glslang::TShader shader(stage);
		shader.setStringsWithLengthsAndNames(&source, nullptr, &sourceName, 1);
		shader.setPreamble(preamble.c_str());
		shader.setEntryPoint(entryPoint);
		shader.setEnvInput(glslang::EShSourceHlsl, stage, glslang::EShClientVulkan, clientInputSemanticsVersion);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

		const TBuiltInResource builtInResources = glslang::DefaultTBuiltInResource;
		const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);
		MyIncluder includer;

		if (!shader.parse(&builtInResources, defaultVersion, false, messages, includer))
		{
			ss << "parse(), " << shader.getInfoLog() << shader.getInfoDebugLog();
			g_ret = ss.str();
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}

		glslang::TProgram program;
		program.addShader(&shader);
		if (!program.link(messages))
		{
			ss << "link(), " << shader.getInfoLog() << shader.getInfoDebugLog();
			g_ret = ss.str();
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}

		if (!program.mapIO())
		{
			ss << "mapIO(), " << shader.getInfoLog() << shader.getInfoDebugLog();
			g_ret = ss.str();
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}

		spv::SpvBuildLogger logger;
		glslang::SpvOptions options;
		if (flags & 0x1)
		{
			options.generateDebugInfo = true;
			options.disableOptimizer = true;
			options.optimizeSize = false;
		}
		else
		{
			options.generateDebugInfo = false;
			options.disableOptimizer = false;
			options.optimizeSize = true;
		}
		glslang::GlslangToSpv(*program.getIntermediate(stage), g_vSpirv, &logger, &options);

		const unsigned int magicNumber = 0x07230203;
		if (g_vSpirv[0] != magicNumber)
		{
			g_ret = "magicNumber";
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}

		*ppCode = g_vSpirv.data();
		*pSize = static_cast<UINT32>(g_vSpirv.size() * sizeof(unsigned int));

		return true;
	}
}
