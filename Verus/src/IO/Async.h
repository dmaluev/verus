// Copyright (C) 2021-2022, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus::IO
{
	struct AsyncDelegate
	{
		virtual void Async_WhenLoaded(CSZ url, RcBlob blob) = 0;
	};
	VERUS_TYPEDEFS(AsyncDelegate);

	// Load resources asynchronously.
	// Load method just adds the url to a queue.
	// Load and Cancel are virtual so that they can be safely called from another DLL.
	// Internally they can allocate and free memory.
	class Async : public Singleton<Async>, public Object, public Lockable
	{
	public:
		struct TaskDesc
		{
			int  _texturePart = 0;
			bool _nullTerm = false;
			bool _checkExist = false;
			bool _runOnMainThread = true;

			TaskDesc(bool nullTerm = false, bool checkExist = false, int texturePart = 0, bool runOnMainThread = true) :
				_nullTerm(nullTerm),
				_checkExist(checkExist),
				_texturePart(texturePart),
				_runOnMainThread(runOnMainThread) {}
		};
		VERUS_TYPEDEFS(TaskDesc);

	private:
		struct Task
		{
			Vector<PAsyncDelegate> _vOwners;
			Vector<BYTE>           _v;
			TaskDesc               _desc;
			bool                   _loaded = false;
		};
		VERUS_TYPEDEFS(Task);

		static const int s_orderLength = 8;

		typedef Map<String, Task> TMapTasks;

		TMapTasks               _mapTasks;
		std::thread             _thread;
		std::condition_variable _cv;
		D::RuntimeError         _ex;
		CSZ                     _queue[256];
		int                     _cursorRead = 0;
		int                     _cursorWrite = 0;
		UINT32                  _order = 0;
		std::atomic_bool        _stopThread;
		bool                    _inUpdate = false;
		bool                    _flush = false;
		bool                    _onePerUpdateMode = false;

	public:
		Async();
		~Async();

		void Init();
		void Done();

		virtual void Load(CSZ url, PAsyncDelegate pDelegate, RcTaskDesc desc = TaskDesc());
		VERUS_P(virtual void _Cancel(PAsyncDelegate pDelegate));
		static void Cancel(PAsyncDelegate pDelegate);

		virtual void Update();

		void Flush();
		void SetOnePerUpdateMode(bool b) { _onePerUpdateMode = b; }

		VERUS_P(void ThreadProc());
	};
	VERUS_TYPEDEFS(Async);
}
