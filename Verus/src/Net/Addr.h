// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Net
	{
		//! Holds IP address and port.
		class Addr
		{
		public:
			union
			{
				UINT32 _addr = 0;
				BYTE   _a[4];
			};
			UINT16 _port = 0;

			Addr() = default;
			Addr(const sockaddr_in& sa);
			Addr(CSZ addr, int port);

			bool operator==(const Addr& that) const;

			//! Returns localhost address 127.0.0.1.
			static Addr Localhost(int port = 0);

			bool IsNull() const { return !_addr || !_port; }
			bool IsLocalhost() const;

			//! Accepts IP:Port or URL, which is resolved using getaddrinfo().
			void FromString(CSZ addr);
			String ToString(bool addPort = false) const;

			sockaddr_in ToSockAddr() const;
		};
		VERUS_TYPEDEFS(Addr);
	}
}
