#pragma once

namespace verus
{
	class Utils : public Singleton<Utils>
	{
		Random         _random;
		String         _modulePath;
		String         _shaderPath;
		String         _writablePath;
		PBaseAllocator _pAllocator;

	public:
		Utils(PBaseAllocator pAlloc);
		~Utils();

		//! Construct Utils using the provided allocator:
		static void MakeEx(PBaseAllocator pAlloc);
		//! Destruct Utils using the provided allocator:
		static void FreeEx(PBaseAllocator pAlloc);

		void InitPaths();
		Str  GetModulePath() const { return _C(_modulePath); }
		void SetModulePath(CSZ path) { _modulePath = path; }
		Str  GetShaderPath() const { return _C(_shaderPath); }
		void SetShaderPath(CSZ path) { _shaderPath = path; }
		Str  GetWritablePath() const { return _C(_writablePath); }
		void SetWritablePath(CSZ path) { _writablePath = path; }

		RRandom GetRandom() { return _random; }

		// System-wide allocator:
		PBaseAllocator GetAllocator() { return _pAllocator; }
		void           SetAllocator(PBaseAllocator p) { _pAllocator = p; }

		static void ExitSdlLoop();

		static INT32 Cast32(INT64 x);
		static UINT32 Cast32(UINT64 x);

		static void CopyColor(BYTE dest[4], UINT32 src);
		static void CopyColor(UINT32& dest, const BYTE src[4]);

		static void CopyByteToInt4(const BYTE src[4], int dest[4]);
		static void CopyIntToByte4(const int src[4], BYTE dest[4]);

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
