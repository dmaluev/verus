#include "verus.h"

namespace verus
{
	void Make_CGI()
	{
		CGI::Renderer::Make();
	}
	void Free_CGI()
	{
		CGI::Renderer::Free();
	}
}
