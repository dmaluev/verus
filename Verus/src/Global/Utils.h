#pragma once

namespace verus
{
	class Utils : public Singleton<Utils>
	{
		Random         _random;
		PBaseAllocator _pAllocator;

	public:
		Utils(PBaseAllocator pAlloc);
		~Utils();

		//! Construct Utils using the provided allocator:
		static void MakeEx(PBaseAllocator pAlloc);
		//! Destruct Utils using the provided allocator:
		static void FreeEx(PBaseAllocator pAlloc);

		RRandom GetRandom() { return _random; }

		// System-wide allocator:
		PBaseAllocator GetAllocator() { return _pAllocator; }
		void           SetAllocator(PBaseAllocator p) { _pAllocator = p; }

		static void ExitSdlLoop();

		static INT32 Cast32(INT64 x);
		static UINT32 Cast32(UINT64 x);

		template<typename T>
		static T* Swap(T*& pDst, T*& pSrc)
		{
			T* pPrev = pDst;
			pDst = pSrc;
			return pPrev;
		}

		static CSZ GetSingletonFailMessage()
		{
			return "verus::Utils::SetupPaths(); // FAIL.\r\ncorr::Utils::MakeEx(&alloc); // FAIL.\r\n";
		}
	};
	VERUS_TYPEDEFS(Utils);
}
