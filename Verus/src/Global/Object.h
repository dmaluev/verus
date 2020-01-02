#pragma once

#ifdef VERUS_RELEASE_DEBUG
#	define VERUS_UPDATE_ONCE_CHECK UpdateOnceCheck()
#	define VERUS_UPDATE_ONCE_CHECK_DRAW UpdateOnceCheckDraw()
#else
#	define VERUS_UPDATE_ONCE_CHECK
#	define VERUS_UPDATE_ONCE_CHECK_DRAW
#endif

#define VERUS_INIT() Object::Init();
#define VERUS_DONE(x) if (Object::IsInitialized()) { Object::Done(); this->~x(); new(this) x(); }

namespace verus
{
	struct ObjectFlags
	{
		enum
		{
			init = (1 << 0),
			user = (1 << 1)
		};
	};

	class Object
	{
		std::atomic_uint _flags;
#ifdef VERUS_RELEASE_DEBUG
		UINT64 _updateOnceFrame = 0;
#endif

	protected:
		Object();
		virtual ~Object();

		void Init();
		void Done();

#ifdef VERUS_RELEASE_DEBUG
		void UpdateOnceCheck();
		void UpdateOnceCheckDraw();
#endif

	public:
		bool IsInitialized() const { return _flags & ObjectFlags::init; }

		bool IsFlagSet(UINT32 mask) const { return !!(_flags & mask); }
		void   SetFlag(UINT32 mask) { _flags |= mask; }
		void ResetFlag(UINT32 mask) { _flags &= ~mask; }
	};
}
