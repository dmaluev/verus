#include "verus.h"

using namespace verus;
using namespace verus::Net;

HttpFile::HttpFile()
{
	_ready = false;
	VERUS_ZERO_MEM(_buffer);
}

HttpFile::~HttpFile()
{
	Close();
}

bool HttpFile::Open(CSZ url)
{
	try
	{
		if (strncmp(url, "http://", schemeLength))
			return false;
		int port = httpPort;

		// Key points:
		CSZ colon = strchr(url + schemeLength, ':');
		CSZ slash = strchr(url + schemeLength, '/');
		CSZ q = strchr(url + schemeLength, '?');

		if (colon)
			port = atoi(colon + 1);

		// Parse host:
		CSZ end = url + strlen(url);
		if (q)
			end = q;
		if (slash)
			end = slash;
		if (colon)
			end = colon;
		String host(url + schemeLength, end);

		_socket.Connect(Addr(_C(host), port));

		// Form request:
		String req("GET ");
		req.reserve(200);
		req += slash ? slash : "/";
		req += " HTTP/1.0" VERUS_CRNL;
		req += "Host: " + host + VERUS_CRNL VERUS_CRNL;

		// Send it:
		_socket.Send(_C(req), Utils::Cast32(req.length()));

		String header;
		header.reserve(400);
		bool done = false;
		while (!done)
		{
			const int size = _socket.Recv(_buffer, sizeof(_buffer));
			if (size > 0)
				header.append(_buffer, _buffer + size);
			const size_t endh = header.find(VERUS_CRNL VERUS_CRNL);
			if (!size)
			{
				done = true;
			}
			if (endh != String::npos)
			{
				_contentPart = Utils::Cast32(header.length() - endh - 4);
				memcpy(_buffer, _C(header) + endh + 4, _contentPart);
				done = true;
			}
		}

		CSZ p = _C(header);
		while (p)
		{
			const size_t span = strcspn(p, VERUS_CRNL);
			if (!span)
				break;
			CSZ end = p + span;
			CSZ colon = strchr(p, ':');
			if (colon > end)
				colon = nullptr;
			String key, value;
			if (colon)
			{
				key.assign(p, colon);
				value.assign(colon + 2, end);
			}
			else
			{
				key.assign("Title");
				value.assign(p, end);
			}

#ifdef _DEBUG
			char info[128];
			sprintf_s(info, "%s: %s", _C(key), _C(value));
			VERUS_LOG_DEBUG(info);
#endif

			_mapHeader[key] = value;
			p += span;
			if (!strncmp(p, VERUS_CRNL VERUS_CRNL, 4))
				break;
			p += strspn(p, VERUS_CRNL);
		}

		_size = atoi(_C(_mapHeader["Content-Length"]));
	}
	catch (D::RcRecoverable)
	{
		return false;
	}
	return true;
}

void HttpFile::Close()
{
	if (_thread.joinable())
		_thread.join();
	_socket.Close();
}

INT64 HttpFile::Read(void* p, INT64 size)
{
	const INT64 inBuffer = Math::Max(_contentPart - _offset, INT64(0));
	const INT64 fromBuffer = Math::Min(inBuffer, size);
	const INT64 fromNet = size - fromBuffer;
	const INT64 end = _offset + size;
	if (fromBuffer)
	{
		memcpy(p, _buffer + _offset, static_cast<size_t>(fromBuffer));
		_offset += fromBuffer;
	}
	const INT64 offsetStart = _offset;
	while (fromNet && _offset != end)
	{
		const int ret = _socket.Recv(
			static_cast<BYTE*>(p) + fromBuffer + (_offset - offsetStart),
			Utils::Cast32(end - _offset));
		if (ret <= 0)
			break;
		_offset += ret;
	}
	return fromBuffer + (_offset - offsetStart);
}

INT64 HttpFile::Write(const void* p, INT64 size)
{
	return 0;
}

INT64 HttpFile::GetSize()
{
	return _size;
}

void HttpFile::DownloadAsync(CSZ url, bool nullTerm)
{
	_url = url;
	_nullTerm = nullTerm;
	_ready = false;
	_thread = std::thread(&HttpFile::ThreadProc, this);
}

void HttpFile::ThreadProc()
{
	SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
	if (Open(_C(_url)))
	{
		const INT64 size = GetSize();
		if (size <= maxContentLength)
		{
			_vData.resize(size_t(_nullTerm ? size + 1 : size));
			Read(_vData.data(), size);
		}
	}
	_ready = true;
}
