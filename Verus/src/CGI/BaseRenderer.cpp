#include "verus.h"

using namespace verus;
using namespace verus::CGI;

BaseRenderer::BaseRenderer()
{
}

BaseRenderer::~BaseRenderer()
{
}

PBaseRenderer BaseRenderer::Load(CSZ dll, RBaseRendererDesc desc)
{
#ifdef _WIN32
	PFNVERUSCREATERENDERER VerusCreateRenderer = reinterpret_cast<PFNVERUSCREATERENDERER>(
		GetProcAddress(LoadLibraryA(dll), "VerusCreateRenderer"));
	return VerusCreateRenderer(VERUS_SDK_VERSION, &desc);
#else
	PFNVERUSCREATERENDERER VerusCreateRenderer = reinterpret_cast<PFNVERUSCREATERENDERER>(
		dlsym(dlopen("./libRenderOpenGL.so", RTLD_LAZY), "VerusCreateRenderer"));
	return VerusCreateRenderer(VERUS_SDK_VERSION, &desc);
#endif
}
