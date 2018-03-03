#include "verus.h"

using namespace verus;
using namespace verus::D;

std::mutex D::Log::s_mutex;

void Log::Error(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line)
{
	std::lock_guard<std::mutex> lock(s_mutex);

	const time_t t = time(0);
	const tm* pTM = localtime(&t);
	char timestamp[80];
	strftime(timestamp, sizeof(timestamp), "%Y.%m.%d %H:%M:%S", pTM);

	CSZ p = strrchr(filename, '/');
	if (!p)
		p = strrchr(filename, '\\');
	filename = p ? p + 1 : filename;

	String pathName;
	//if (Global::IsValidSingleton())
	//{
	//	pathName = Global::I().GetWritablePath() + "/Errors.txt";
	//}
	//else
	{
		String temp = getenv("TEMP");
		pathName = temp + "/CorrErrors.txt";
	}

	IO::File file;
	if (file.Open(_C(pathName), "a"))
	{
		if (file.GetSize() > 100 * 1024)
		{
			file.Close();
			file.Open(_C(pathName), "w");
		}
		StringStream ss;
		ss << timestamp << " " << tid << " " << filename << ":" << line << " ERROR: " << txt << "\n";
		file.Write(_C(ss.str()), ss.str().length());
	}
}

void Log::Session(CSZ txt)
{
	/*
	std::lock_guard<std::mutex> lock(s_mutex);

	String pathName(Global::I().GetWritablePath());
	pathName += "/SessionLog.txt";
	IO::File file;
	if (file.Create(_C(pathName), "a"))
	{
		StringStream ss;
		ss.fill('0');
		ss << '[';
		ss.width(8);
		ss << SDL_GetTicks() << "ms] " << txt << "\n";
		file.Write(_C(ss.str()), ss.str().length());
	}
	*/
}

void Log::DebugString(CSZ txt)
{
	/*
	StringStream ss;
	ss << verus::Global::GetNextCounterValue() << ": " << txt << "\n";

#ifdef _WIN32
	OutputDebugStringA(_C(ss.str()));
#endif
	*/
}

void Log::Write(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line, Severity severity)
{
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
