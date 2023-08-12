// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::CGI
{
	class Scheduled
	{
		UINT64 _doneFrame = UINT64_MAX;

	public:
		virtual Continue Scheduled_Update() { return Continue::no; }

		void Schedule(INT64 extraFrameCount = -1);
		void ForceScheduled();
		bool IsScheduledAllowed();
	};
	VERUS_TYPEDEFS(Scheduled);
}
