// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Net;

Addr::Addr(const sockaddr_in& sa)
{
	_addr = ntohl(sa.sin_addr.S_un.S_addr);
	_port = ntohs(sa.sin_port);
}

Addr::Addr(CSZ addr, int port)
{
	_port = port;
	if (!addr)
		_addr = 0;
	else
		FromString(addr); // This can update port.
}

bool Addr::operator==(RcAddr that) const
{
	return
		_addr == that._addr &&
		_port == that._port;
}

Addr Addr::Localhost(int port)
{
	Addr addr;
	addr._addr = 0x7F000001;
	addr._port = port;
	return addr;
}

bool Addr::IsLocalhost() const
{
	return 0x7F000001 == _addr;
}

void Addr::FromString(CSZ addr)
{
	char addrBuff[40];
	CSZ pColon = strchr(addr, ':');
	if (pColon) // "IP:Port" format?
	{
		const int len = Math::Min<int>(Utils::Cast32(pColon - addr), sizeof(addrBuff) - 1);
		strncpy(addrBuff, addr, len);
		addrBuff[len] = 0;
		_port = atoi(addr + len + 1);
		addr = addrBuff;
	}

	const int ret = inet_pton(AF_INET, addr, &_addr);
	if (1 != ret)
	{
		struct RAII
		{
			addrinfo* pAddrInfo;

			RAII() : pAddrInfo(nullptr) {}
			~RAII()
			{
				if (pAddrInfo)
				{
					freeaddrinfo(pAddrInfo);
					pAddrInfo = nullptr;
				}
			}
		} raii;

		addrinfo hints = {};
		hints.ai_family = AF_INET;
		const int ret = getaddrinfo(addr, 0, &hints, &raii.pAddrInfo);
		if (ret)
		{
			throw VERUS_RECOVERABLE << "getaddrinfo(), " << ret;
		}
		else
		{
			const UINT16 port = _port;
			const addrinfo* pAI = nullptr;
			for (pAI = raii.pAddrInfo; pAI; pAI = pAI->ai_next)
			{
				if (PF_INET == pAI->ai_family)
				{
					*this = Addr(*reinterpret_cast<const sockaddr_in*>(pAI->ai_addr));
					break;
				}
			}
			_port = port;
		}
	}
	else
		_addr = ntohl(_addr);
}

String Addr::ToString(bool addPort) const
{
	sockaddr_in sa;
	sa.sin_addr.S_un.S_addr = htonl(_addr);
	char s[64] = {};
	inet_ntop(AF_INET, &sa.sin_addr, s, sizeof(s));
	return String(s) + (addPort ? ":" + std::to_string(_port) : "");
}

sockaddr_in Addr::ToSockAddr() const
{
	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_addr.S_un.S_addr = htonl(_addr);
	sa.sin_port = htons(_port);
	return sa;
}
