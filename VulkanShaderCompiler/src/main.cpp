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

bool g_init = false;
std::string g_ret;
std::vector<unsigned int> g_vSpirv;

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
		return nullptr;
	}

	virtual IncludeResult* includeLocal(
		CSZ headerName,
		CSZ includerName,
		size_t inclusionDepth) override
	{
		return nullptr;
	}

	virtual void releaseInclude(IncludeResult* result) override
	{
	}
};

extern "C"
{
	VERUS_DLL_EXPORT void VerusCompilerInit()
	{
		if (!g_init)
		{
			g_init = true;
			glslang::InitializeProcess();
		}
	}

	VERUS_DLL_EXPORT void VerusCompilerDone()
	{
		if (g_init)
		{
			g_init = false;
			glslang::FinalizeProcess();
		}
	}

	VERUS_DLL_EXPORT bool VerusCompile(CSZ source, CSZ* defines, CSZ entryPoint, CSZ target, UINT32 flags, UINT32** ppCode, UINT32* pSize, CSZ* ppErrorMsgs)
	{
		if (!g_init)
		{
			g_ret = "Call InitializeProcess";
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}

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
		std::string preamble = ssDefines.str();

		glslang::TShader shader(stage);
		shader.setStrings(&source, 1);
		shader.setPreamble(preamble.c_str());
		shader.setEntryPoint(entryPoint);
		shader.setEnvInput(glslang::EShSourceHlsl, stage, glslang::EShClientVulkan, 100);
		shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
		shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);

		const TBuiltInResource builtInResources = glslang::DefaultTBuiltInResource;
		const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
		std::string preprocessed;
		MyIncluder includer;

		if (!shader.preprocess(&builtInResources, 450, ENoProfile, false, false, messages, &preprocessed, includer))
		{
			ss << "preprocess(), " << shader.getInfoLog() << shader.getInfoDebugLog();
			g_ret = ss.str();
			*ppErrorMsgs = g_ret.c_str();
			return false;
		}
		CSZ pPreprocessed = preprocessed.c_str();
		shader.setStrings(&pPreprocessed, 1);

		if (!shader.parse(&builtInResources, 450, false, messages))
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

		spv::SpvBuildLogger logger;
		glslang::SpvOptions options;
		glslang::GlslangToSpv(*program.getIntermediate(stage), g_vSpirv, &logger, &options);

		*ppCode = g_vSpirv.data();
		*pSize = static_cast<UINT32>(g_vSpirv.size());

		return true;
	}
}
