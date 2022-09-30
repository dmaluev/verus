// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace IO
	{
		class Vwx : public AsyncDelegate
		{
		public:
			Vwx();
			~Vwx();

			virtual void Async_WhenLoaded(CSZ url, RcBlob blob) override;

			void Serialize(CSZ url);
			void Deserialize(CSZ url, bool sync);

			void Deserialize_LegacyXXX(CSZ url, RcBlob blob);
		};
	}
}
