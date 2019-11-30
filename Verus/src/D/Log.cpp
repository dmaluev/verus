#include "verus.h"

using namespace verus;
using namespace verus::D;

std::mutex D::Log::s_mutex;

void Log::Write(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line, Severity severity)
{
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

	IO::File file;
	if (file.Open("Log.txt", "a"))
	{
		if (file.GetSize() > 100 * 1024)
		{
			file.Close();
			file.Open("Log.txt", "w");
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
