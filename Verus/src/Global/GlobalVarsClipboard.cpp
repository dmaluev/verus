#include "verus.h"

using namespace verus;

void GlobalVarsClipboard::Copy()
{
	_vPairs.reserve(8);

	_vPairs.push_back(Pair(0, Utils::P()));
	_vPairs.push_back(Pair(1, App::Settings::P()));
	_vPairs.push_back(Pair(2, CGI::Renderer::P()));
	_vPairs.push_back(Pair(3, IO::Async::P()));
}

void GlobalVarsClipboard::Paste()
{
	Utils::Assign(static_cast<Utils*>(_vPairs[0]._p));
	App::Settings::Assign(static_cast<App::Settings*>(_vPairs[1]._p));
	CGI::Renderer::Assign(static_cast<CGI::Renderer*>(_vPairs[2]._p));
	IO::Async::Assign(static_cast<IO::Async*>(_vPairs[3]._p));
}
