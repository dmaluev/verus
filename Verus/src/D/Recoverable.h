#pragma once

namespace verus
{
	namespace D
	{
		class Recoverable : public RuntimeError
		{
		public:
			Recoverable(std::thread::id tid = std::this_thread::get_id(), CSZ file = __FILE__, UINT32 line = __LINE__) : RuntimeError(tid, file, line) {}
			Recoverable(const Recoverable& that)
			{
				*this = that;
			}
			Recoverable& operator=(const Recoverable& that)
			{
				RuntimeError::operator=(that);
				return *this;
			}

			template<typename T>
			Recoverable& operator<<(const T& t)
			{
				RuntimeError::operator<<(t);
				return *this;
			}
		};
		VERUS_TYPEDEFS(Recoverable);
	}
}

#define VERUS_RECOVERABLE verus::D::Recoverable(std::this_thread::get_id(), __FILE__, __LINE__)