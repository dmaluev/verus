#pragma once

namespace verus
{
	//! A convenient way to store a collection of objects.

	//! It's a good idea not to use new/delete explicitly. Use STL allocator.
	//! 'Unique' class uses map and supports reference counting.
	//! The Insert method returns a raw pointer which should be wrapped inside a Ptr.
	//!
	//! Types of pointers:
	//! - raw pointer - very dumb, no automatic initialization.
	//! - Ptr wrapper - like a smart pointer, but even smarter.
	//! - Pwn wrapper - owning Ptr, basically calls Done in destructor.
	//! Use Ptr for function parameters. Use Pwn for class members.
	//!
	template<typename TValue>
	class Store
	{
	protected:
		typedef List<TValue> TList;

		TList _list;

	public:
		TValue* Insert()
		{
			_list.emplace_back();
			return &_list.back();
		}

		void Delete(TValue* p)
		{
			_list.remove_if([p](const TValue& v)
				{
					return p == &v;
				});
		}

		void DeleteAll()
		{
			_list.clear();
		}

		TValue& GetStoredAt(int index)
		{
			auto it = _list.begin();
			std::advance(it, index);
			return *it;
		}

		int FindIndex(TValue* p)
		{
			int index = 0;
			for (const auto& x : _list)
			{
				if (&x == p)
					return index;
				index++;
			}
			return -1;
		}

		int GetStoredCount() const
		{
			return static_cast<int>(_list.size());
		}
	};

	template<typename TKey, typename TValue>
	class StoreUnique
	{
	protected:
		typedef Map<TKey, TValue> TMap;

		TMap _map;

	public:
		TValue* Insert(const TKey& key)
		{
			VERUS_IF_FOUND_IN(TMap, _map, key, it)
			{
				it->second.AddRef();
				return &it->second;
			}
			return &_map[key];
		}

		TValue* Find(const TKey& key)
		{
			VERUS_IF_FOUND_IN(TMap, _map, key, it)
				return &it->second;
			return nullptr;
		}

		void Delete(const TKey& key)
		{
			VERUS_IF_FOUND_IN(TMap, _map, key, it)
			{
				if (it->second.Done())
					_map.erase(it);
			}
		}

		void DeleteAll()
		{
			for (auto& x : _map)
			{
				while (!x.second.Done());
			}
			_map.clear();
		}

		TValue& GetStoredAt(int index)
		{
			auto it = _map.begin();
			std::advance(it, index);
			return *it;
		}

		int GetStoredCount() const
		{
			return static_cast<int>(_map.size());
		}
	};

	template<typename TKey, typename TValue>
	class StoreUniqueWithNoRefCount
	{
	protected:
		typedef Map<TKey, TValue> TMap;

		TMap _map;

	public:
		TValue* Insert(const TKey& key)
		{
			return &_map[key];
		}

		TValue* Find(const TKey& key)
		{
			VERUS_IF_FOUND_IN(TMap, _map, key, it)
				return &it->second;
			return nullptr;
		}

		void Delete(const TKey& key)
		{
			VERUS_IF_FOUND_IN(TMap, _map, key, it)
				_map.erase(it);
		}

		void DeleteAll()
		{
			_map.clear();
		}

		TValue& GetStoredAt(int index)
		{
			auto it = _map.begin();
			std::advance(it, index);
			return *it;
		}

		int GetStoredCount() const
		{
			return static_cast<int>(_map.size());
		}
	};

	template<typename T>
	class Ptr
	{
	protected:
		T* _p;

	public:
		Ptr(T* p = nullptr) : _p(p) {}
		~Ptr() {}

		T& operator*() const { return *_p; }
		T* operator->() const { return _p; }
		explicit operator bool() const { return !!_p; }
		bool operator==(const Ptr<T>& that) const { return _p == that._p; }
		bool operator!=(const Ptr<T>& that) const { return _p != that._p; }
		bool operator<(const Ptr<T>& that) const { return _p < that._p; }

		T* Attach(T* ptr)
		{
			T* p = _p;
			_p = ptr;
			return p;
		}

		T* Detach()
		{
			T* p = _p;
			_p = nullptr;
			return p;
		}

		T* Get() const
		{
			return _p;
		}
	};

	template<typename T, int COUNT>
	class Pwns
	{
		T _t[COUNT];

	public:
		Pwns()
		{
		}

		~Pwns()
		{
			Done();
		}

		void Done()
		{
			VERUS_FOR(i, COUNT)
				_t[i].Done();
		}

		const T& operator[](int i) const
		{
			VERUS_RT_ASSERT(i >= 0 && i < COUNT);
			return _t[i];
		}
		T& operator[](int i)
		{
			VERUS_RT_ASSERT(i >= 0 && i < COUNT);
			return _t[i];
		}
	};
}
