// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

#define VERUS_LOCK(o) std::unique_lock<std::mutex> lock = std::unique_lock<std::mutex>((o).GetMutex())
#define VERUS_LOCK_EX(o, no) std::unique_lock<std::mutex> lock = no ?\
	std::unique_lock<std::mutex>((o).GetMutex(), std::defer_lock) : std::unique_lock<std::mutex>((o).GetMutex())

namespace verus
{
	class Lockable
	{
		std::mutex _mutex;

	public:
		std::mutex& GetMutex() { return _mutex; }
	};
	VERUS_TYPEDEFS(Lockable);
}
