// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::Net;

//#define VERUS_SOCKET_LOG

WSADATA Socket::s_wsaData;

// Socket::Client:

void Socket::Client::ThreadProc()
{
	const int size = Utils::Cast32(_vBuffer.size());
	int read = 0;
	Vector<BYTE> vLocal;
	vLocal.resize(size);
	int ret = 1;
	while (ret > 0)
	{
		ret = recv(_socket, reinterpret_cast<char*>(&vLocal[read]), size - read, 0);
		if (ret > 0)
			read += ret;
		if (read = size)
		{
			VERUS_LOCK(*_pListener);
			memcpy(_vBuffer.data(), vLocal.data(), size);
			read = 0;
		}
	}

	closesocket(_socket);

	{
		VERUS_LOCK(*_pListener);
		_pListener = nullptr;
		_socket = INVALID_SOCKET;
	}
}

// Socket:

Socket::Socket()
{
}

Socket::~Socket()
{
	Close();
}

void Socket::Startup()
{
	VERUS_LOG_INFO("Startup()");
	WSAStartup(MAKEWORD(2, 2), &s_wsaData);
}

void Socket::Cleanup()
{
	VERUS_LOG_INFO("Cleanup()");
	WSACleanup();
}

void Socket::Listen(int port)
{
	VERUS_RT_ASSERT(INVALID_SOCKET == _socket);

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

	int ret = 0;

	addrinfo hints = {};
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	char porttext[16];
	sprintf_s(porttext, "%d", port);
	if (ret = getaddrinfo(0, porttext, &hints, &raii.pAddrInfo))
		throw VERUS_RECOVERABLE << "getaddrinfo(); " << ret;

	_socket = socket(
		raii.pAddrInfo->ai_family,
		raii.pAddrInfo->ai_socktype,
		raii.pAddrInfo->ai_protocol);
	if (INVALID_SOCKET == _socket)
		throw VERUS_RECOVERABLE << "socket(); " << WSAGetLastError();

	if (bind(_socket,
		raii.pAddrInfo->ai_addr,
		Utils::Cast32(raii.pAddrInfo->ai_addrlen)))
	{
		closesocket(_socket);
		throw VERUS_RECOVERABLE << "bind(); " << WSAGetLastError();
	}

	if (listen(_socket, SOMAXCONN))
	{
		closesocket(_socket);
		throw VERUS_RECOVERABLE << "listen(); " << WSAGetLastError();
	}

	_vClients.resize(_maxClients);
	_thread = std::thread(&Socket::ThreadProc, this);
}

void Socket::Connect(RcAddr addr)
{
	VERUS_RT_ASSERT(INVALID_SOCKET == _socket);
	_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (_socket != INVALID_SOCKET)
	{
		const sockaddr_in sa = addr.ToSockAddr();
		const int ret = connect(_socket, reinterpret_cast<const sockaddr*>(&sa), sizeof(sa));
		if (SOCKET_ERROR != ret)
			return;
		else
		{
			closesocket(_socket);
			throw VERUS_RECOVERABLE << "connect(); " << WSAGetLastError();
		}
	}
	else
		throw VERUS_RECOVERABLE << "socket(); " << WSAGetLastError();
}

void Socket::Udp(int port)
{
	VERUS_RT_ASSERT(INVALID_SOCKET == _socket);
	_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (_socket != INVALID_SOCKET)
	{
		sockaddr_in sa = {};
		sa.sin_family = AF_INET;
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		sa.sin_port = htons(port);
		if (bind(_socket, reinterpret_cast<const sockaddr*>(&sa), sizeof(sa)) < 0)
		{
			closesocket(_socket);
			throw VERUS_RECOVERABLE << "bind(); " << WSAGetLastError();
		}
		// Non-blocking mode:
		u_long mode = 1;
		if (ioctlsocket(_socket, FIONBIO, &mode))
		{
			closesocket(_socket);
			throw VERUS_RECOVERABLE << "ioctlsocket(FIONBIO); " << WSAGetLastError();
		}
	}
	else
		throw VERUS_RECOVERABLE << "socket(); " << WSAGetLastError();
}

void Socket::Close()
{
	if (_socket != INVALID_SOCKET)
	{
		closesocket(_socket);
		_socket = INVALID_SOCKET;
	}

	CloseAllClients();

	if (_thread.joinable())
		_thread.join();
}

int Socket::Send(const void* p, int size) const
{
	if (!size)
		size = Utils::Cast32(strlen(static_cast<CSZ>(p)));
	return send(_socket, static_cast<CSZ>(p), size, 0);
}

int Socket::SendTo(const void* p, int size, RcAddr addr) const
{
	sockaddr_in sa = addr.ToSockAddr();
	if (!size)
		size = Utils::Cast32(strlen(static_cast<CSZ>(p)));
#ifdef VERUS_SOCKET_LOG
	VERUS_LOG_SESSION(_C("sendto, " + addr.ToString(true) + ", " + std::to_string(size) + ", [0]=" + std::to_string(((BYTE*)p)[0])));
#endif
	return sendto(_socket, static_cast<CSZ>(p), size, 0, reinterpret_cast<const sockaddr*>(&sa), sizeof(sa));
}

int Socket::Recv(void* p, int size) const
{
	return recv(_socket, static_cast<SZ>(p), size, 0);
}

int Socket::RecvFrom(void* p, int size, RAddr addr) const
{
	sockaddr_in sa = {};
	int saLen = sizeof(sa);
	const int ret = recvfrom(_socket, static_cast<SZ>(p), size, 0, reinterpret_cast<sockaddr*>(&sa), &saLen);
	addr = Addr(sa);
#ifdef VERUS_SOCKET_LOG
	if (ret > 0)
		VERUS_LOG_SESSION(_C("recvfrom, " + addr.ToString(true) + ", " + std::to_string(ret) + ", [0]=" + std::to_string(((BYTE*)p)[0])));
#endif
	return ret;
}

void Socket::ThreadProc()
{
	SOCKET s = 0;
	while (s != INVALID_SOCKET)
	{
		s = accept(_socket, 0, 0);
		if (s != INVALID_SOCKET)
		{
			VERUS_LOCK(*this);
			const int slot = GetFreeClientSlot();
			if (slot >= 0)
			{
				_vClients[slot] = new Client;
				PClient pClient = _vClients[slot];
				pClient->_pListener = this;
				pClient->_socket = s;
				pClient->_vBuffer.resize(_clientBufferSize);
				pClient->_thread = std::thread(&Client::ThreadProc, pClient);
			}
			else
			{
				closesocket(s);
			}
		}
	}
}

void Socket::GetLatestClientBuffer(int id, BYTE* p)
{
	VERUS_LOCK(*this);
	if ((int)_vClients.size() > id && _vClients[id] && _vClients[id]->_pListener)
		memcpy(p, _vClients[id]->_vBuffer.data(), _clientBufferSize);
}

int Socket::GetFreeClientSlot() const
{
	VERUS_FOR(i, _maxClients)
	{
		if (!_vClients[i])
			return i;
	}
	return -1;
}

void Socket::CloseAllClients()
{
	VERUS_RT_ASSERT(INVALID_SOCKET == _socket);

	{
		VERUS_LOCK(*this);
		VERUS_FOREACH(Vector<PClient>, _vClients, it)
			if (*it)
				closesocket((*it)->_socket);
	}
	VERUS_FOREACH(Vector<PClient>, _vClients, it)
	{
		if (*it && (*it)->_thread.joinable())
			(*it)->_thread.join();
	}
	VERUS_FOREACH(Vector<PClient>, _vClients, it)
	{
		if (*it)
			delete* it;
	}
	_vClients.clear();
}
