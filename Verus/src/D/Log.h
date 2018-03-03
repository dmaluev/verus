#pragma once

//#define VERUS_LOG_ERROR(txt)			{verus::D::Log::Error(txt, std::this_thread::get_id(), __FILE__, __LINE__);}
//#define VERUS_LOG_ERROR_STREAM(txt)		{StringStream ss; ss << txt; verus::D::Log::Error(_C(ss.str()), std::this_thread::get_id(), __FILE__, __LINE__);}
//#define VERUS_LOG_SESSION(txt)			{verus::D::Log::Session(txt);}
//#define VERUS_LOG_SESSION_STREAM(txt)	{StringStream ss; ss << txt; verus::D::Log::Session(_C(ss.str()));}
//#ifdef _DEBUG
//#	define VERUS_LOG_DEBUG(x) {verus::D::Log::DebugString(x);}
//#else
//#	define VERUS_LOG_DEBUG(x) {}
//#endif

#define VERUS_LOG_ERROR(txt) {StringStream ss; ss << txt; D::Log::I().Write(_C(ss.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::error);}
#define VERUS_LOG_WARN(txt)  {StringStream ss; ss << txt; D::Log::I().Write(_C(ss.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::warning);}
#define VERUS_LOG_INFO(txt)  {StringStream ss; ss << txt; D::Log::I().Write(_C(ss.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::info);}
#define VERUS_LOG_DEBUG(txt) {StringStream ss; ss << txt; D::Log::I().Write(_C(ss.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::debug);}

namespace verus
{
	namespace D
	{
		class Log : public Singleton<Log>
		{
		public:
			enum class Severity : int
			{
				error,
				warning,
				info,
				debug
			};

		private:
			static std::mutex s_mutex;

		public:
			static void Error(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line);
			static void Session(CSZ txt);
			static void DebugString(CSZ txt);

			void Write(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line, Severity severity);

			static void FormatTime(char* buffer, size_t size);
			static CSZ GetSeverityLetter(Severity severity);
			static String ExtractFilename(CSZ filename);
		};
	}
}
