// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;

void GlobalVarsClipboard::Copy()
{
	_vPairs.reserve(8);

	_vPairs.push_back(Pair(0, Utils::P()));
	_vPairs.push_back(Pair(1, D::Log::P()));
	_vPairs.push_back(Pair(2, App::Settings::P()));
	_vPairs.push_back(Pair(3, CGI::Renderer::P()));
	_vPairs.push_back(Pair(4, IO::Async::P()));
	_vPairs.push_back(Pair(5, Input::InputManager::P()));
}

void GlobalVarsClipboard::Paste()
{
	Utils::Assign(static_cast<Utils*>(_vPairs[0]._p));
	D::Log::Assign(static_cast<D::Log*>(_vPairs[1]._p));
	App::Settings::Assign(static_cast<App::Settings*>(_vPairs[2]._p));
	CGI::Renderer::Assign(static_cast<CGI::Renderer*>(_vPairs[3]._p));
	IO::Async::Assign(static_cast<IO::Async*>(_vPairs[4]._p));
	Input::InputManager::Assign(static_cast<Input::InputManager*>(_vPairs[5]._p));
}
