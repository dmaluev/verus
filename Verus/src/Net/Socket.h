// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Net
	{
		class Socket : public Lockable
		{
			class Client
			{
				friend class Socket;

				Socket* _pListener = nullptr;
				SOCKET       _socket = INVALID_SOCKET;
				std::thread  _thread;
				Vector<BYTE> _vBuffer;

				void ThreadProc();
			};
			VERUS_TYPEDEFS(Client);

			static WSADATA  s_wsaData;
			SOCKET          _socket = INVALID_SOCKET;
			std::thread     _thread;
			Vector<PClient> _vClients;
			int             _maxClients = 1;
			int             _clientBufferSize = 0;

		public:
			Socket();
			~Socket();

			static void Startup();
			static void Cleanup();

			void Listen(int port);
			void Connect(RcAddr addr);
			void Udp(int port);
			void Close();

			int   Send(const void* p, int size) const;
			int SendTo(const void* p, int size, RcAddr addr) const;
			int     Recv(void* p, int size) const;
			int RecvFrom(void* p, int size, RAddr addr) const;

			void SetMaxClients(int count) { _maxClients = count; }
			void SetClientBufferSize(int size) { _clientBufferSize = size; }

			VERUS_P(void ThreadProc());

			void GetLatestClientBuffer(int id, BYTE* p);

			VERUS_P(int GetFreeClientSlot() const);

			VERUS_P(void CloseAllClients());
		};
		VERUS_TYPEDEFS(Socket);
	}
}
