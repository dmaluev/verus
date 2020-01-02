#pragma once

extern int g_singletonAllocCount;

namespace verus
{
	//! Restricts the instantiation of a class to one object.
	template<typename T>
	class Singleton : public AllocatorAware
	{
		static T* s_pSingleton;

		Singleton(const Singleton& that);
		Singleton& operator=(const Singleton& that);

	public:
		Singleton()
		{
			s_pSingleton = static_cast<T*>(this);
		}

		virtual ~Singleton()
		{
		}

		static inline void Make()
		{
			if (s_pSingleton)
				return;
			new T();
			g_singletonAllocCount++;
		}

		static inline void Free()
		{
			if (s_pSingleton)
			{
				g_singletonAllocCount--;
				delete s_pSingleton;
			}
			s_pSingleton = nullptr;
		}

		static inline T& I() { Test(); return *s_pSingleton; }
		static inline T* P() { Test(); return s_pSingleton; }

		static inline const T& IConst() { Test(); return *s_pSingleton; }
		static inline const T* PConst() { Test(); return s_pSingleton; }

		static inline bool IsValidSingleton()
		{
			return s_pSingleton != nullptr;
		}

		static inline void Assign(T* p) { s_pSingleton = p; }

		static inline void Test()
		{
#ifdef _DEBUG
			//if (!IsValidSingleton())
			//	VERUS_LOG_DEBUG(T::GetSingletonFailMessage());
			//VERUS_RT_ASSERT(s_pSingleton);
#endif
		};

		static inline CSZ GetSingletonFailMessage() { return "Singleton FAIL.\r\n"; }

		static inline void TestAllocCount()
		{
			//VERUS_RT_ASSERT(!g_singletonAllocCount);
		}
	};
	template<typename T> T* Singleton<T>::s_pSingleton = nullptr;
}
