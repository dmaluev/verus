#pragma once

namespace verus
{
	namespace CGI
	{
		class Scheduled
		{
			INT64 _frame = -1;

		public:
			virtual Continue Scheduled_Update() { return Continue::no; }

			void Schedule(INT64 frameCount = -1);
			void ForceScheduled();
			bool IsScheduledAllowed();
		};
		VERUS_TYPEDEFS(Scheduled);
	}
}
