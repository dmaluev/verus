// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	class Utils : public Singleton<Utils>
	{
		Random         _random;
		String         _companyFolderName;
		String         _writableFolderName;
		String         _modulePath;
		String         _shaderPath;
		String         _writablePath;
		String         _projectPath;
		PBaseAllocator _pAllocator;

	public:
		Utils(PBaseAllocator pAlloc);
		~Utils();

		//! Construct Utils using the provided allocator:
		static void MakeEx(PBaseAllocator pAlloc);
		//! Destruct Utils using the provided allocator:
		static void FreeEx(PBaseAllocator pAlloc);

		// Called from BaseGame::Initialize():
		void InitPaths();

		Str  GetCompanyFolderName() const { return _C(_companyFolderName); }
		void SetCompanyFolderName(CSZ name) { _companyFolderName = name; }
		Str  GetWritableFolderName() const { return _C(_writableFolderName); }
		void SetWritableFolderName(CSZ name) { _writableFolderName = name; }
		Str  GetModulePath() const { return _C(_modulePath); }
		void SetModulePath(CSZ path) { _modulePath = path; }
		Str  GetShaderPath() const { return _C(_shaderPath); }
		void SetShaderPath(CSZ path) { _shaderPath = path; }
		Str  GetWritablePath() const { return _C(_writablePath); }
		void SetWritablePath(CSZ path) { _writablePath = path; }
		Str  GetProjectPath() const { return _C(_projectPath); }
		void SetProjectPath(CSZ path) { _projectPath = path; }

		RRandom GetRandom() { return _random; }

		// System-wide allocator:
		PBaseAllocator GetAllocator() { return _pAllocator; }
		void           SetAllocator(PBaseAllocator p) { _pAllocator = p; }

		static void PushQuitEvent();

		static void OpenUrl(CSZ url)
		{
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)
#else
			const WideString urlW = Str::Utf8ToWide(url);
			ShellExecute(0, L"open", _C(urlW), 0, 0, SW_SHOWNORMAL);
#endif
		}

		static INT32 Cast32(INT64 x);
		static UINT32 Cast32(UINT64 x);

		static void CopyColor(BYTE dest[4], UINT32 src);
		static void CopyColor(UINT32& dest, const BYTE src[4]);

		static void CopyByteToInt4(const BYTE src[4], int dest[4]);
		static void CopyIntToByte4(const int src[4], BYTE dest[4]);

		static void TestAll();

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
