// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace Net
	{
		struct GameDesc
		{
			String _name;
			String _map;
			Addr   _addr;
			UINT16 _version = 0;
			UINT16 _count = 0;
		};
		VERUS_TYPEDEFS(GameDesc);

		enum { REPORT_ID_SIZE = 1 };

		enum REPORT_ID
		{
			REPORT_NOOP,
			REPORT_RNAT,
			REPORT_RACK,
			REPORT_PADD,
			REPORT_PREM,
			REPORT_KICK,
			REPORT_USER
		};

		struct MultiplayerDelegate
		{
			virtual void Multiplayer_OnReportFrom/**/(int id, const BYTE* p) = 0;
			virtual bool Multiplayer_IsReliable  /**/(const BYTE* p) = 0;
			virtual bool Multiplayer_OnConnect   /**/(int id, const BYTE* p, RcAddr addr) = 0;
			virtual void Multiplayer_OnDisconnect/**/(int id) = 0;
		};
		VERUS_TYPEDEFS(MultiplayerDelegate);

		struct MultiplayerFlags
		{
			enum
			{
				sendReports = (ObjectFlags::user << 0),
				activeGamesReady = (ObjectFlags::user << 1),
				noLock = (ObjectFlags::user << 2),
				running = (ObjectFlags::user << 3)
			};
		};

		class Multiplayer : public Singleton<Multiplayer>, public Object, public Lockable
		{
			enum class Seq : UINT16
			{
				skip,
				unreliable,
				reliableBase
			};

			class Player
			{
				friend class Multiplayer;

				std::chrono::steady_clock::time_point _tpLast;
				String                                _log;
				BaseCircularBuffer                    _sendBuffer;
				BaseCircularBuffer                    _recvBuffer;
				Addr                                  _addr; // Not null means active player.
				UINT32                                _statSendBufferFullCount = 0;
				UINT32                                _statRecvBufferFullCount = 0;
				UINT16                                _sendSeq = +Seq::reliableBase;
				UINT16                                _sendSeqResend = +Seq::reliableBase;
				UINT16                                _recvSeq = +Seq::reliableBase;
				bool                                  _reserved = false; // Taken by app for bot, etc.

			public:
				Player();

				void Reset();
				void Log(CSZ msg);
				void SaveLog(int id, bool server);
			};
			VERUS_TYPEDEFS(Player);

			static const int s_bufferSize = 256;
			static const int s_logSize = 20 * 1024;

			PMultiplayerDelegate    _pDelegate = nullptr;
			Vector<Player>          _vPlayers;
			Vector<BYTE>            _vMasterBuffer;
			Vector<BYTE>            _vReportBuffer;
			Vector<GameDesc>        _vGameDesc;
			Vector<Addr>            _vNat;
			String                  _lobbyWebsite;
			String                  _lobbyGameID;
			GameDesc                _lobbyGameDesc;
			Socket                  _socket;
			std::thread             _thread;
			std::thread             _threadWeb;
			std::condition_variable _cv;
			int                     _maxPlayers = 0;
			int                     _maxReportSize = 0;
			int                     _maxReports = 0;
			int                     _myPlayer = -1;
			Addr                    _serverAddr; // Localhost means that this machine is the server.

			static bool IsLessThan(UINT16 a, UINT16 b);
			static bool IsGreaterThan(UINT16 a, UINT16 b);
			static void NextSeq(UINT16& seq);

		public:
			Multiplayer();
			~Multiplayer();

			void Init(RcAddr addr, int maxPlayers, int maxReportSize, int maxReports = 32);
			void Done();

			PMultiplayerDelegate SetDelegate(PMultiplayerDelegate p) { return Utils::Swap(_pDelegate, p); }

			bool IsServer() const { return _serverAddr.IsLocalhost(); }
			RcAddr GetServerAddr() const { return _serverAddr; }
			int GetMyPlayer() const { return _myPlayer; }
			void SetMyPlayer(int id) { _myPlayer = id; }
			int GetPlayerCount();
			int GetMaxPlayers() const { return _maxPlayers; }
			bool IsActivePlayer(int id);

			int ReservePlayer();
			void CancelReservation(int id);

			// Writes the report into the send buffer for some or all players. This will eventually send this report.
			// id = Player's ID. Use -1 to send the report to all players. Server cannot send the report to itself.
			// p = Pointer to report's data. Must not be less than the size of the report.
			// deferred = Will send it as soon as possible if false.
			void SendReportAsync(int id, BYTE* p, bool deferred = false);
			// Reads reports from recv buffers, cleans them, calls MultiplayerDelegate's callbacks.
			void ProcessRecvBuffers();
			// Writes the KICK report to the player's send buffer. This will eventually kick the specified player.
			void Kick(int id);

			VERUS_P(int GetIdByAddr(RcAddr addr) const);
			VERUS_P(int FindFreeID() const);
			VERUS_P(void ThreadProc());

			String GetDebug();

			void UploadActiveGameAsync(RcGameDesc gd, CSZ gameID, CSZ website = "swiborg.com");
			void DownloadActiveGamesAsync(CSZ gameID, int version, CSZ website = "swiborg.com");
			bool AreActiveGamesReady();
			void UploadNatRequest(CSZ gameID, CSZ website = "swiborg.com");
			Vector<GameDesc> GetActiveGames();
			VERUS_P(void ThreadProcWeb());
		};
		VERUS_TYPEDEFS(Multiplayer);
	}
}
