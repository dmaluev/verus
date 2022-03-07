// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "pch.h"

using namespace verus;

int main(VERUS_MAIN_DEFAULT_ARGS)
{
	Game::HelloTexturedCubeGame game;
	try
	{
		game.Initialize(argc, argv);
		game.Loop();
	}
	catch (D::RcRuntimeError e)
	{
		D::Log::I().Write(e.what(), e.GetThreadID(), e.GetFile(), e.GetLine(), D::Log::Severity::error);
		return EXIT_FAILURE;
	}
	catch (const std::exception& e)
	{
		VERUS_LOG_ERROR(e.what());
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
