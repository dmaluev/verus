#pragma once

namespace verus
{
	namespace D
	{
		//! See: https://marknelson.us/posts/2007/11/13/no-exceptions.html
		class RuntimeError : public std::exception
		{
			mutable StringStream _ss;
			mutable String       _what;
			std::thread::id      _tid;
			String               _file;
			UINT32               _line = 0;

		public:
			RuntimeError(std::thread::id tid = std::this_thread::get_id(), CSZ file = __FILE__, UINT32 line = __LINE__) :
				_tid(tid), _line(line)
			{
				CSZ p = strrchr(file, '/');
				if (!p)
					p = strrchr(file, '\\');
				_file = p ? p + 1 : file;
			}
			RuntimeError(const RuntimeError& that)
			{
				*this = that;
			}
			RuntimeError& operator=(const RuntimeError& that)
			{
				_what += that._what;
				_what += that._ss.str();
				_tid = that._tid;
				_file = that._file;
				_line = that._line;
				return *this;
			}
			virtual ~RuntimeError() throw() {}

			virtual CSZ what() const throw()
			{
				if (_ss.str().size())
				{
					_what += _ss.str();
					_ss.str("");
				}
				return _C(_what);
			}

			std::thread::id GetThreadID() const { return _tid; }
			CSZ GetFile() const { return _C(_file); }
			UINT32 GetLine() const { return _line; }

			template<typename T>
			RuntimeError& operator<<(const T& t)
			{
				_ss << t;
				return *this;
			}

			bool IsRaised() const { return !!strlen(what()); }
		};
		VERUS_TYPEDEFS(RuntimeError);
	}
}

#define VERUS_RUNTIME_ERROR verus::D::RuntimeError(std::this_thread::get_id(), __FILE__, __LINE__)
