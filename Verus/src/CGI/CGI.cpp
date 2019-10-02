#include "verus.h"

namespace verus
{
	void Make_CGI()
	{
		CGI::Renderer::Make();
		CGI::DebugDraw::Make();
	}
	void Free_CGI()
	{
		CGI::DebugDraw::Free();
		CGI::Renderer::Free();
	}
}
