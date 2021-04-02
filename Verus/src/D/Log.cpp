// Copyright (C) 2021, Dmitry Maluev (dmaluev@gmail.com). All rights reserved.
#include "verus.h"

using namespace verus;
using namespace verus::D;

void Log::Write(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line, Severity severity)
{
	if (strstr(txt, "VUID-VkMappedMemoryRange-size-01389"))
		return;
	if (strstr(txt, "VUID-StandaloneSpirv-Offset-04663"))
		return;
	if (strstr(txt, "UNASSIGNED-BestPractices-vkAllocateMemory-small-allocation"))
		return;
	if (strstr(txt, "UNASSIGNED-BestPractices-vkBindMemory-small-dedicated-allocation"))
		return;
	if (strstr(txt, "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed"))
		return;

	VERUS_LOCK(*this);

#ifdef _DEBUG
	if (severity <= Severity::error)
	{
		SDL_TriggerBreakpoint();
	}
#endif
	char timestamp[40];
	FormatTime(timestamp, sizeof(timestamp));
	CSZ severityLetter = GetSeverityLetter(severity);
	const String filenameEx = ExtractFilename(filename);
	StringStream ss;
	ss << timestamp << " [" << severityLetter << "] [" << tid << "] [" << filenameEx << ":" << line << "] " << txt << std::endl;
	const String s = ss.str();

	if (_pathname.empty())
	{
		_pathname += _C(Utils::I().GetWritablePath());
		_pathname += "Log.txt";
	}

	IO::File file;
	if (file.Open(_C(_pathname), "a"))
	{
		if (file.GetSize() > 100 * 1024)
		{
			file.Close();
			file.Open(_C(_pathname), "w");
		}
		file.Write(_C(s), s.length());
	}
}

void Log::FormatTime(char* buffer, size_t size)
{
	const auto now = std::chrono::system_clock::now();
	const time_t tnow = std::chrono::system_clock::to_time_t(now);
	const auto trimmed = std::chrono::system_clock::from_time_t(tnow);
	const int ms = static_cast<int>(std::chrono::duration_cast<std::chrono::microseconds>(now - trimmed).count());
	char msText[16];
	sprintf_s(msText, ".%06d", ms);
	tm* pTM = localtime(&tnow);
	strftime(buffer, size, "%Y-%m-%d %H:%M:%S", pTM);
	strcat(buffer, msText);
}

CSZ Log::GetSeverityLetter(Severity severity)
{
	switch (severity)
	{
	case Severity::error: return "E";
	case Severity::warning: return "W";
	case Severity::info: return "I";
	case Severity::debug: return "D";
	}
	return "X";
}

String Log::ExtractFilename(CSZ filename)
{
	CSZ p = strrchr(filename, '/');
	if (!p)
		p = strrchr(filename, '\\');
	filename = p ? p + 1 : filename;
	p = strrchr(filename, '.');
	return p ? String(filename, p) : String(filename);
}
