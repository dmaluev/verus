#pragma once

namespace verus
{
	namespace IO
	{
		class StreamPtr : public Stream
		{
			const BYTE* _p;
			INT64       _size;
			INT64       _offset;

		public:
			StreamPtr(RcBlob blob) : _p(blob._p), _size(blob._size), _offset(0) {}
			~StreamPtr() {}

			INT64 GetOffset() const { return _offset; }
			INT64 GetSize() const { return _size; }
			bool IsEnd() const { return _offset >= _size; }

			void Advance(INT64 a) { _offset += a; }

			virtual INT64 Read(void* p, INT64 size) override
			{
				VERUS_RT_ASSERT(_offset + size <= _size);
				memcpy(p, _p + _offset, size);
				_offset += size;
				return size;
			}

			virtual INT64 Write(const void* p, INT64 size) override
			{
				VERUS_RT_FAIL("Write()");
				return 0;
			}
		};
		VERUS_TYPEDEFS(StreamPtr);
	}
}
