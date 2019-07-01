#pragma once

namespace verus
{
	namespace CGI
	{
		class DestroyStaging
		{
			INT64 _frame = -1;

		public:
			void Schedule()
			{
				_frame = Renderer::I().GetNumFrames() + BaseRenderer::s_ringBufferSize + (Utils::I().GetRandom().Next() & 0xFF);
			}

			void Allow()
			{
				_frame = 0;
			}

			bool IsAllowed()
			{
				if (-1 == _frame)
					return false;
				if (static_cast<INT64>(Renderer::I().GetNumFrames()) < _frame)
					return false;
				_frame = -1;
				return true;
			}
		};
	}
}
