// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

//#define VERUS_MP_BAD_CONNECTION

#ifdef VERUS_MP_BAD_CONNECTION
#	pragma message("WARNING! Multiplayer in bad connection testing mode!")
#endif

using namespace verus;
using namespace verus::Net;

// Multiplayer::Player:

Multiplayer::Player::Player()
{
	Reset();
}

void Multiplayer::Player::Reset()
{
	_log.clear();
	_log.reserve(s_logSize);
	_statSendBufferFullCount = 0;
	_statRecvBufferFullCount = 0;
	_sendSeq = +Seq::reliableBase;
	_sendSeqResend = +Seq::reliableBase;
	_recvSeq = +Seq::reliableBase;
	_sendBuffer.Clear();
	_recvBuffer.Clear();
	_addr = Addr();
	_reserved = false;
}

void Multiplayer::Player::Log(CSZ msg)
{
	if (_log.length() >= s_logSize)
		return;
	const time_t t = time(nullptr);
	const tm* pTM = localtime(&t);
	char buffer[80];
	strftime(buffer, sizeof(buffer), "%H:%M:%S", pTM);
	_log += buffer;
	_log += ": ";
	_log += msg;
	_log += "\n";
}

void Multiplayer::Player::SaveLog(int id, bool server)
{
	CSZ serv = server ? " (SERVER)" : "";
	StringStream ss;
	ss << _C(Utils::I().GetWritablePath()) << "/Multiplayer (" << id << ")" << serv << ".log";
	IO::File file;
	file.Open(_C(ss.str()), "w");
	file.Write(_C(_log), _log.length());
}

// Multiplayer:

bool Multiplayer::IsLessThan(UINT16 a, UINT16 b)
{
	const int diff = a - b;
	return (diff < 0 && diff > -SHRT_MAX) || diff > SHRT_MAX;
}

bool Multiplayer::IsGreaterThan(UINT16 a, UINT16 b)
{
	const int diff = a - b;
	return (diff > 0 && diff < SHRT_MAX) || diff < -SHRT_MAX;
}

void Multiplayer::NextSeq(UINT16& seq)
{
	seq++;
	if (seq < +Seq::reliableBase)
		seq = +Seq::reliableBase;
}

Multiplayer::Multiplayer()
{
	VERUS_CT_ASSERT(1 == REPORT_ID_SIZE);
	SetFlag(MultiplayerFlags::activeGamesReady);
}

Multiplayer::~Multiplayer()
{
	Done();
}

void Multiplayer::Init(RcAddr addr, int maxPlayers, int maxReportSize, int maxReports)
{
	VERUS_INIT();

	_serverAddr = addr;
	_maxPlayers = maxPlayers;
	_maxReportSize = maxReportSize;
	_maxReports = maxReports;
	const int cacheBytesPerPlayer = _maxReportSize * _maxReports;
	_vMasterBuffer.resize(cacheBytesPerPlayer * _maxPlayers * 2); // 2x for send + receive.
	_vReportBuffer.resize(_maxReportSize);
	_vPlayers.resize(_maxPlayers);
	//VERUS_OUTPUT_DEBUG_STRING(_C(String("MP: master = ") + std::to_string(_vMasterBuffer.size()) +
	//	", report = " + std::to_string(_vReportBuffer.size()) + ", players = " + std::to_string(_vPlayers.size())));
	int id = 0;
	for (auto& player : _vPlayers)
	{
		const int offsetS = (2 * id + 0) * cacheBytesPerPlayer;
		const int offsetR = (2 * id + 1) * cacheBytesPerPlayer;
		player._sendBuffer = BaseCircularBuffer(_vMasterBuffer.data() + offsetS, _maxReports, _maxReportSize);
		player._recvBuffer = BaseCircularBuffer(_vMasterBuffer.data() + offsetR, _maxReports, _maxReportSize);
		//VERUS_OUTPUT_DEBUG_STRING(_C(String("MP: ID=") + std::to_string(id) +
		//	" layout: send @ " + std::to_string(offsetS) + ", recv @ " + std::to_string(offsetR)));
		id++;
	}
	_vNat.reserve(8);
	int port = _serverAddr._port;
	if (!_serverAddr.IsLocalhost()) // Use port N+1, if client:
		port++;
	_vPlayers[0]._addr = _serverAddr;
	_vPlayers[0]._tpLast = std::chrono::steady_clock::now();
	_socket.Udp(port);
	_thread = std::thread(&Multiplayer::ThreadProc, this);
}

void Multiplayer::Done()
{
	if (_thread.joinable())
	{
		// Send kick to localhost:
		Addr addr = Addr::Localhost();
		addr._port = _serverAddr._port;
		if (!_serverAddr.IsLocalhost()) // Use port N+1, if client:
			addr._port++;
		const BYTE kick = REPORT_KICK;
		_socket.SendTo(&kick, 1, addr);

		_cv.notify_one();
		_thread.join();
	}
	if (_threadWeb.joinable())
		_threadWeb.join();
	_socket.Close();
	VERUS_DONE(Multiplayer);
}

int Multiplayer::GetPlayerCount()
{
	VERUS_LOCK(*this);
	int count = 0;
	for (const auto& player : _vPlayers)
	{
		if (!player._addr.IsNull())
			count++;
	}
	return count;
}

bool Multiplayer::IsActivePlayer(int id)
{
	VERUS_LOCK(*this);
	return !_vPlayers[id]._addr.IsNull();
}

int Multiplayer::ReservePlayer()
{
	VERUS_LOCK(*this);
	int id = 0;
	for (auto& player : _vPlayers)
	{
		if (player._addr.IsNull() && !player._reserved)
		{
			player._reserved = true;
			return id;
		}
		id++;
	}
	return -1;
}

void Multiplayer::CancelReservation(int id)
{
	VERUS_LOCK(*this);
	VERUS_RT_ASSERT(_vPlayers[id]._reserved && _vPlayers[id]._addr.IsNull());
	_vPlayers[id].Reset();
}

void Multiplayer::SendReportAsync(int id, BYTE* p, bool deferred)
{
	{
		VERUS_LOCK_EX(*this, IsFlagSet(MultiplayerFlags::noLock));
		const bool reliable = _pDelegate->Multiplayer_IsReliable(p);
		VERUS_FOR(i, _maxPlayers)
		{
			PPlayer pPlayer = &_vPlayers[i];

			if (id >= 0)
				pPlayer = &_vPlayers[id];
			else if (!IsServer())
				pPlayer = &_vPlayers[0];
			else if (IsServer())
			{
				if (0 == i)
					continue; // Do not send message to localhost.
			}

			if (pPlayer->_addr.IsNull())
				continue;

			if (pPlayer->_sendBuffer.IsFull()) // Full? Remove unreliable:
			{
				pPlayer->_statSendBufferFullCount++;
				pPlayer->Log("Send buffer is full, cleaning");
				UINT16 seq = +Seq::unreliable;
				BYTE* p = pPlayer->_sendBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
				while (p)
				{
					pPlayer->_sendBuffer.MoveToFront(p);
					pPlayer->_sendBuffer.Pop(nullptr);
					p = pPlayer->_sendBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
					if (!p)
					{
						seq = +Seq::skip; // Now clean 'skip'...
						p = pPlayer->_sendBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
					}
				}
			}

			if (!reliable) // Send it and forget it?
			{
				const UINT16 seq = +Seq::unreliable;
				memcpy(p + REPORT_ID_SIZE, &seq, 2);
				BYTE* pMatch = pPlayer->_sendBuffer.FindItem(p, REPORT_ID_SIZE);
				if (pMatch) // Just overwrite existing report?
				{
					memcpy(pMatch, p, pPlayer->_sendBuffer.GetMaxItemSize());
					continue;
				}
			}
			else
			{
				if (pPlayer->_sendBuffer.IsFull())
				{
					pPlayer->Log("Send buffer is full, report is lost");
					continue; // Next player...
				}
				memcpy(p + REPORT_ID_SIZE, &pPlayer->_sendSeq, 2);
				NextSeq(pPlayer->_sendSeq);
			}
			pPlayer->_sendBuffer.Push(p);

			if (id >= 0 || !IsServer())
				break;
		}
		SetFlag(MultiplayerFlags::sendReports); // ThreadProc() should know about the new report.
	}
	if (!deferred)
		_cv.notify_one(); // Wake up!
}

void Multiplayer::ProcessRecvBuffers()
{
	VERUS_LOCK(*this);
	int id = 0;
	for (auto& player : _vPlayers)
	{
		if (!player._addr.IsNull())
		{
			UINT16 seq;
			BYTE* p;

			SetFlag(MultiplayerFlags::noLock);

			// 1) Remove all 'skip':
			seq = +Seq::skip;
			while (p = player._recvBuffer.FindItem(&seq, sizeof(seq), REPORT_ID_SIZE))
			{
				player._recvBuffer.MoveToFront(p);
				player._recvBuffer.Pop(nullptr);
			}

			// 2) Get & remove all 'unreliable':
			seq = +Seq::unreliable;
			while (p = player._recvBuffer.FindItem(&seq, sizeof(seq), REPORT_ID_SIZE))
			{
				player._recvBuffer.MoveToFront(p);
				player._recvBuffer.Pop(_vReportBuffer.data());
				switch (_vReportBuffer[0])
				{
				case REPORT_PADD:
				{
					player._recvBuffer.Pop(_vReportBuffer.data(), true); // JOIN?
					if (!_pDelegate->Multiplayer_OnConnect(id, _vReportBuffer.data(), player._addr))
					{
						player.SaveLog(id, IsServer());
						player.Reset(); // App rejects this player?
					}
				}
				break;
				case REPORT_PREM:
				{
					_pDelegate->Multiplayer_OnDisconnect(id);
					player.SaveLog(id, IsServer());
					player.Reset(); // Forget this player.
				}
				break;
				default:
				{
					_pDelegate->Multiplayer_OnReportFrom(id, _vReportBuffer.data());
				}
				}
			}

			// 3) Get 'reliable':
			seq = player._recvSeq;
			while (p = player._recvBuffer.FindItem(&seq, sizeof(seq), REPORT_ID_SIZE))
			{
				player._recvBuffer.MoveToFront(p);
				player._recvBuffer.Pop(_vReportBuffer.data());
				_pDelegate->Multiplayer_OnReportFrom(id, _vReportBuffer.data());
				NextSeq(player._recvSeq); // Expect next sequence number.
				seq = player._recvSeq;
			}

			ResetFlag(MultiplayerFlags::noLock);

			if (player._recvBuffer.IsFull()) // Bad condition: no space for the expected sequence number!
			{
				int count = player._recvBuffer.GetItemCount();
				bool detected = false;
				while (count && player._recvBuffer.Pop(_vReportBuffer.data()))
				{
					memcpy(&seq, &_vReportBuffer[REPORT_ID_SIZE], 2);
					if (!IsLessThan(seq, player._recvSeq))
						player._recvBuffer.Push(_vReportBuffer.data());
					else
						detected = true;
					count--;
				}
				if (detected)
					player.Log("Invalid sequence number detected in recv buffer");
				NextSeq(player._recvSeq); // Expect next sequence number (data loss!).
			}
		}
		id++;
	}
}

void Multiplayer::Kick(int id)
{
	BYTE kick[3];
	kick[0] = REPORT_KICK;
	const UINT16 seq = +Seq::unreliable;
	memcpy(kick + REPORT_ID_SIZE, &seq, 2);
	SendReportAsync(id, kick);
	// Now the ThreadProc() should send the KICK report and push PREM report.
}

int Multiplayer::GetIdByAddr(RcAddr addr) const
{
	int id = 0;
	for (const auto& player : _vPlayers)
	{
		if (player._addr == addr)
			return id;
		id++;
	}
	return -1;
}

int Multiplayer::FindFreeID() const
{
	int id = 0;
	for (const auto& player : _vPlayers)
	{
		if (player._addr.IsNull() && !player._reserved)
			return id;
		id++;
	}
	return -1;
}

void Multiplayer::ThreadProc()
{
	VERUS_RT_ASSERT(IsInitialized());
	VERUS_LOCK(*this);
	Addr addr;
	int res;
	BYTE* p;
	UINT16 seq;
	UINT16 size;
	std::chrono::steady_clock::time_point tpResend = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point tpNat = std::chrono::steady_clock::now();
	while (true)
	{
		_cv.wait_for(lock, std::chrono::milliseconds(10)); // ~100 times per second.

		bool resend = false;
		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - tpResend).count() >= 3)
		{
			tpResend = std::chrono::steady_clock::now();
			resend = true;
		}

		bool nat = false;
		if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - tpNat).count() >= 5)
		{
			tpNat = std::chrono::steady_clock::now();
			nat = true;
		}

		if (nat) // Send data to the clients which will allow them to send data to the server:
		{
			const BYTE report = REPORT_RNAT;
			for (const auto& natAddr : _vNat)
				_socket.SendTo(&report, sizeof(report), natAddr);
			//VERUS_OUTPUT_DEBUG_STRING(_C(String("MP: NAT Punch-through addresses: " + std::to_string(_vNat.size()))));
		}

		// Need to send/resend some reports?
		if (IsFlagSet(MultiplayerFlags::sendReports) || resend)
		{
			for (auto& player : _vPlayers)
			{
				if (player._addr.IsNull())
					continue;

				bool remove = false;

				// 1) Remove all 'skip' (acknowledged reports):
				seq = +Seq::skip;
				while (p = player._sendBuffer.FindItem(&seq, sizeof(seq), REPORT_ID_SIZE))
				{
					player._sendBuffer.MoveToFront(p);
					player._sendBuffer.Pop(nullptr);
				}

				// 2) Send & remove all 'unreliable':
				seq = +Seq::unreliable;
				while (p = player._sendBuffer.FindItem(&seq, sizeof(seq), REPORT_ID_SIZE))
				{
					player._sendBuffer.MoveToFront(p);
					player._sendBuffer.Pop(_vReportBuffer.data());
					if (REPORT_KICK == _vReportBuffer[0]) // Player was kicked?
					{
						size = 3;
						remove = true;
					}
					else
						memcpy(&size, &_vReportBuffer[3], 2);
					res = _socket.SendTo(_vReportBuffer.data(), size, player._addr);
					player.Log(_C("Sending unreliable report " + std::to_string(_vReportBuffer[0])));
				}

				// 3) Send 'reliable', do not remove:
				int count = player._sendBuffer.GetItemCount();
				while (count && player._sendBuffer.Pop(_vReportBuffer.data()))
				{
					memcpy(&seq, &_vReportBuffer[REPORT_ID_SIZE], 2);
					if (seq == player._sendSeqResend || IsGreaterThan(seq, player._sendSeqResend) || resend)
					{
						memcpy(&size, &_vReportBuffer[3], 2);
#ifdef VERUS_MP_BAD_CONNECTION // Emulate lost report condition:
						if (!(Utils::I().GetRandom().Next() % 2))
#endif
						{
							res = _socket.SendTo(_vReportBuffer.data(), size, player._addr);
#ifdef VERUS_MP_BAD_CONNECTION // Emulate duplicate report condition:
							res = _socket.SendTo(_vReportBuffer.data(), size, player._addr);
#endif
							player.Log(_C("Sending reliable report " + std::to_string(_vReportBuffer[0])));
						}
					}
					player._sendBuffer.Push(_vReportBuffer.data()); // Push it back in!
					count--;
				}
				player._sendSeqResend = player._sendSeq;

				// Disconnect idle / kicked:
				if (IsServer() && !player._addr.IsLocalhost())
				{
#ifdef _DEBUG
					const int deadline = 3600;
#else
					const int deadline = 101;
#endif
					if (std::chrono::duration_cast<std::chrono::seconds>(
						std::chrono::steady_clock::now() - player._tpLast).count() >= deadline ||
						remove)
					{
						BYTE prem[3];
						prem[0] = REPORT_PREM;
						seq = +Seq::unreliable;
						memcpy(prem + REPORT_ID_SIZE, &seq, 2);
						player._recvBuffer.Clear();
						player._recvBuffer.Push(prem, sizeof(prem));
						if (remove)
							player.Log("Kicked");
						else
							player.Log("Timeout");
						continue;
					}
				}
			}
			ResetFlag(MultiplayerFlags::sendReports);
		}

		// Receive reports from the net?
		while ((res = _socket.RecvFrom(_vReportBuffer.data(), static_cast<int>(_vReportBuffer.size()), addr)) > 0)
		{
			// Start analyzing incoming traffic...

			if (addr.IsLocalhost())
				return; // A report from myself means it is time to exit.

			int id = GetIdByAddr(addr);

			if (id < 0) // Unknown IP. Register new player?
			{
				id = FindFreeID();
				if (id >= 0)
				{
					//VERUS_OUTPUT_DEBUG_STRING(_C(String("MP: new player " + std::to_string(id))));

					_vPlayers[id].Reset();
					_vPlayers[id]._addr = addr;

					// This will call OnConnect() in ProcessRecvBuffers():
					BYTE padd[3];
					padd[0] = REPORT_PADD;
					seq = +Seq::unreliable;
					memcpy(padd + REPORT_ID_SIZE, &seq, 2);
					_vPlayers[id]._recvBuffer.Push(padd, sizeof(padd));
					_vPlayers[id].Log("Registered");
				}
			}

			if (id >= 0)
			{
				RPlayer player = _vPlayers[id];
				player._tpLast = std::chrono::steady_clock::now(); // Connection is not lost!

				if (player._recvBuffer.IsFull()) // Full? Remove unreliable:
				{
					player._statRecvBufferFullCount++;
					player.Log("Recv buffer is full, cleaning");
					seq = +Seq::unreliable;
					p = player._recvBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
					while (p)
					{
						player._recvBuffer.MoveToFront(p);
						player._recvBuffer.Pop(nullptr);
						p = player._recvBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
						if (!p)
						{
							seq = +Seq::skip; // Now clean 'skip'...
							p = player._recvBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
						}
					}
				}

				bool push = false;
				if (_pDelegate->Multiplayer_IsReliable(_vReportBuffer.data()))
				{
					memcpy(&seq, &_vReportBuffer[REPORT_ID_SIZE], 2); // Check sequence number.
					p = player._recvBuffer.FindItem(&seq, 2, REPORT_ID_SIZE);
					push = !p && (seq == player._recvSeq || IsGreaterThan(seq, player._recvSeq)); // Do not push old reports & duplicates.

					if (!player._recvBuffer.IsFull()) // Send RACK if we can store this report right now:
					{
						//VERUS_OUTPUT_DEBUG_STRING(_C(String("MP: reliable report " + std::to_string(_vReportBuffer[0]) + " from " + std::to_string(id))));
						BYTE rack[4]; // {REPORT_RACK, seq, id}.
						rack[0] = REPORT_RACK;
						memcpy(rack + REPORT_ID_SIZE, &_vReportBuffer[REPORT_ID_SIZE], 2);
						rack[3] = _vReportBuffer[0];
						_socket.SendTo(rack, sizeof(rack), addr); // Send report acknowledgment. Yes, report is received!
						player.Log(_C("Sending RACK for " + std::to_string(_vReportBuffer[0])));
					}
					else
						push = false;
				}
				else
					push = player._recvBuffer.GetItemCount() * 100 / player._recvBuffer.GetMaxItems() < 75; // App will handle the sequence of unreliable reports.

				if (4 == res && REPORT_RACK == _vReportBuffer[0]) // Is this a report acknowledgment? Update send buffer.
				{
					//VERUS_OUTPUT_DEBUG_STRING(_C(String("MP: report ACK " + std::to_string(_vReportBuffer[3]) + " from " + std::to_string(id))));

					BYTE key[3];
					key[0] = _vReportBuffer[3];
					memcpy(key + REPORT_ID_SIZE, &_vReportBuffer[REPORT_ID_SIZE], 2);
					p = player._sendBuffer.FindItem(key, sizeof(key));
					if (p)
					{
						seq = +Seq::skip;
						memcpy(p + REPORT_ID_SIZE, &seq, 2); // Stop sending this report.
					}
					push = false; // RACK should never be seen by the app.
					player.Log(_C("Received RACK for " + std::to_string(key[0])));
				}

				if (push && res >= 5) // Send it to app only if it is unreliable or with proper sequence number:
				{
					memcpy(&size, &_vReportBuffer[3], 2);
					if (res == size) // Valid size?
					{
						player._recvBuffer.Push(_vReportBuffer.data());
						player.Log(_C("Received report " + std::to_string(_vReportBuffer[0]) + " for ProcessRecvBuffers()"));
					}
				}
			}
		}
	}
}

String Multiplayer::GetDebug()
{
	StringStream ss;
	ss << "DEBUG MP: count players = " << GetPlayerCount() << "; ";
	ss << "my ID = " << _myPlayer << "; details:";
	VERUS_LOCK(*this);
	int id = 0;
	for (const auto& player : _vPlayers)
	{
		if (!(id % 4))
			ss << " ";
		if (!player._reserved)
			ss << (player._addr.IsNull() ? "-" : "+");
		else
			ss << "x";
		id++;
	}
	return ss.str();
}

void Multiplayer::UploadActiveGameAsync(RcGameDesc gd, CSZ gameID, CSZ website)
{
	if (IsFlagSet(MultiplayerFlags::running))
		return;
	SetFlag(MultiplayerFlags::running);
	_lobbyGameDesc = gd;
	_lobbyWebsite = website;
	_lobbyGameID = gameID;
	if (_threadWeb.joinable())
		_threadWeb.join();
	_threadWeb = std::thread(&Multiplayer::ThreadProcWeb, this);
}

void Multiplayer::DownloadActiveGamesAsync(CSZ gameID, int version, CSZ website)
{
	if (IsFlagSet(MultiplayerFlags::running))
		return;
	SetFlag(MultiplayerFlags::running);
	ResetFlag(MultiplayerFlags::activeGamesReady);
	_lobbyGameDesc._addr = Addr();
	_lobbyGameDesc._version = version;
	_lobbyWebsite = website;
	_lobbyGameID = gameID;
	if (_threadWeb.joinable())
		_threadWeb.join();
	_threadWeb = std::thread(&Multiplayer::ThreadProcWeb, this);
}

bool Multiplayer::AreActiveGamesReady()
{
	return IsFlagSet(MultiplayerFlags::activeGamesReady);
}

void Multiplayer::UploadNatRequest(CSZ gameID, CSZ website)
{
	if (IsFlagSet(MultiplayerFlags::running))
		return;
	SetFlag(MultiplayerFlags::running);
	_lobbyGameDesc._addr = Addr();
	_lobbyGameDesc._addr._port = 1;
	_lobbyWebsite = website;
	_lobbyGameID = gameID;
	if (_threadWeb.joinable())
		_threadWeb.join();
	_threadWeb = std::thread(&Multiplayer::ThreadProcWeb, this);
}

Vector<GameDesc> Multiplayer::GetActiveGames()
{
	VERUS_LOCK(*this);
	return _vGameDesc;
}

void Multiplayer::ThreadProcWeb()
{
	struct RAII
	{
		Multiplayer& mp;

		RAII(Multiplayer& x) : mp(x) {}
		~RAII() { mp.ResetFlag(MultiplayerFlags::running); }
	} raii(*this);

	try
	{
		if (_lobbyGameDesc._addr.IsNull())
		{
			StringStream ss;
			ss << "GET /game-api/lobby/?ip=0&pid=" << _lobbyGameID << "&ver=" << _lobbyGameDesc._version;
			ss << " HTTP/1.0\r\nHost: " << _lobbyWebsite << "\r\nConnection: close\r\n\r\n";
			Net::Socket s;
			s.Connect(Addr(_C(_lobbyWebsite), 80));
			s.Send(_C(ss.str()), 0);
			String data;
			int ret = 0;
			do
			{
				char buff[s_bufferSize];
				ret = s.Recv(buff, sizeof(buff));
				if (ret > 0)
					data.append(buff, buff + ret);
			} while (ret > 0);

			CSZ p = strstr(_C(data), "\r\n\r\n");

			pugi::xml_document doc;
			const pugi::xml_parse_result result = doc.load_buffer(p, strlen(p));
			if (!result)
				return;
			pugi::xml_node root = doc.first_child();
			if (!root)
				return;

			{
				VERUS_LOCK(*this);
				_vGameDesc.clear();
				if (_vGameDesc.capacity() < 16)
					_vGameDesc.reserve(16);
				for (auto node : root.children("game"))
				{
					GameDesc gameDesc;
					gameDesc._name = node.text().as_string();
					gameDesc._map = node.attribute("map").value();
					gameDesc._addr.FromString(node.attribute("ip").value());
					gameDesc._version = node.attribute("var").as_int();
					gameDesc._count = node.attribute("p").as_int();
					_vGameDesc.push_back(gameDesc);
				}
				SetFlag(MultiplayerFlags::activeGamesReady);
			}
		}
		else
		{
			if (1 == _lobbyGameDesc._addr._port)
			{
				StringStream ss;
				ss << "GET /game-api/nat/?id=" << Str::UrlEncode(_C(_serverAddr.ToString())) << "&port=" << _serverAddr._port + 1 << " HTTP/1.0\r\n";
				ss << "Host: " << _lobbyWebsite << "\r\nConnection: close\r\n\r\n";
				Net::Socket s;
				s.Connect(Addr(_C(_lobbyWebsite), 80));
				s.Send(_C(ss.str()), 0);
				String data;
				int ret = 0;
				do
				{
					char buff[s_bufferSize];
					ret = s.Recv(buff, sizeof(buff));
					if (ret > 0)
						data.append(buff, buff + ret);
				} while (ret > 0);
			}
			else
			{
				StringStream ss;
				ss << "GET /game-api/lobby/?pid=" << _lobbyGameID << "&ver=" << _lobbyGameDesc._version <<
					"&map=" << Str::UrlEncode(_C(_lobbyGameDesc._map)) << "&name=" << Str::UrlEncode(_C(_lobbyGameDesc._name)) << "&p=" <<
					_lobbyGameDesc._count << "&port=" << _serverAddr._port << " HTTP/1.0\r\n";
				ss << "Host: " << _lobbyWebsite << "\r\nConnection: close\r\n\r\n";
				Net::Socket s;
				s.Connect(Addr(_C(_lobbyWebsite), 80));
				s.Send(_C(ss.str()), 0);
				String data;
				int ret = 0;
				do
				{
					char buff[s_bufferSize];
					ret = s.Recv(buff, sizeof(buff));
					if (ret > 0)
						data.append(buff, buff + ret);
				} while (ret > 0);

				CSZ p = strstr(_C(data), "\r\n\r\n");
				if (p)
				{
					VERUS_LOCK(*this);
					_vNat.clear();
					StringStream ss(p + 4);
					String str;
					ss >> str;
					int count = 0;
					while (!str.empty())
					{
						const size_t pos = str.find(':');
						if (pos != String::npos)
							_vNat.push_back(Addr(_C(str.substr(0, pos)), atoi(&str[pos + 1])));
						count++;
						if (20 == count)
							break;
						str.clear();
						ss >> str;
					}
				}
			}
		}
	}
	catch (D::RcRecoverable)
	{
		// No internet?
	}
}
