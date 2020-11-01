// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#pragma once

namespace verus
{
	namespace IO
	{
		class Stream
		{
			UINT32 _version = 0;

		public:
			static const int s_bufferSize = UCHAR_MAX + 1;

			Stream() {}
			virtual ~Stream() {}

			virtual BYTE* GetPointer() { return nullptr; }

			virtual INT64 Read(void* p, INT64 size) = 0;
			virtual INT64 Write(const void* p, INT64 size) = 0;

			INT64 ReadString(SZ sz)
			{
				BYTE len;
				(*this) >> len;
				const INT64 ret = Read(sz, len);
				sz[len] = 0;
				return ret + 1;
			}

			INT64 WriteString(CSZ sz)
			{
				const size_t len = strlen(sz);
				if (len > UCHAR_MAX)
					throw verus::D::RuntimeError(std::this_thread::get_id(), "Stream.h", __LINE__) << "WriteString(), Invalid string length";
				(*this) << static_cast<BYTE>(len);
				const INT64 ret = Write(sz, len);
				return ret + 1;
			}

			void WriteText(CSZ sz)
			{
				Write(sz, strlen(sz));
			}

			Stream& operator<<(CSZ txt)
			{
				WriteText(txt);
				return *this;
			}

			template<typename T> Stream& operator<<(const T& x)
			{
				Write(&x, sizeof(T));
				return *this;
			}

			template<typename T> Stream& operator>>(T& x)
			{
				Read(&x, sizeof(T));
				return *this;
			}

			UINT32 GetVersion() const { return _version; }
			void SetVersion(UINT32 version) { _version = version; }
		};
		VERUS_TYPEDEFS(Stream);

		class SeekableStream : public Stream
		{
			INT64 _blockOffset = 0;

		public:
			SeekableStream() {}
			virtual ~SeekableStream() {}

			virtual void Seek(INT64 offset, int origin) = 0;
			virtual INT64 GetPosition() = 0;

			// Adds a placeholder for block size and begins a block:
			void BeginBlock()
			{
				VERUS_RT_ASSERT(!_blockOffset);
				_blockOffset = GetPosition();
				const INT64 zero = 0;
				Write(&zero, sizeof(zero));
			}

			// Ends a block by placing it's size into placeholder:
			void EndBlock()
			{
				VERUS_RT_ASSERT(_blockOffset);
				const INT64 offset = GetPosition();
				const INT64 delta = offset - _blockOffset - sizeof(INT64);
				Seek(_blockOffset, SEEK_SET);
				Write(&delta, sizeof(delta));
				Seek(0, SEEK_END);
				_blockOffset = 0;
			}

			INT64 GetSize()
			{
				const INT64 was = GetPosition();
				Seek(0, SEEK_END);
				const INT64 size = GetPosition();
				Seek(was, SEEK_SET);
				return size;
			}

			void ReadAll(Vector<BYTE>& vData, bool nullTerm = false)
			{
				const INT64 size = GetSize();
				vData.resize(nullTerm ? size + 1 : size);
				Read(vData.data(), size);
			}
		};
		VERUS_TYPEDEFS(SeekableStream);
	}
}
