#pragma once

#define VERUS_LOG_ERROR(txt) {StringStream ss_Log; ss_Log << txt; D::Log::I().Write(_C(ss_Log.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::error);}
#define VERUS_LOG_WARN(txt)  {StringStream ss_Log; ss_Log << txt; D::Log::I().Write(_C(ss_Log.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::warning);}
#define VERUS_LOG_INFO(txt)  {StringStream ss_Log; ss_Log << txt; D::Log::I().Write(_C(ss_Log.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::info);}
#define VERUS_LOG_DEBUG(txt) {StringStream ss_Log; ss_Log << txt; D::Log::I().Write(_C(ss_Log.str()), std::this_thread::get_id(), __FILE__, __LINE__, D::Log::Severity::debug);}

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
			std::mutex  _mutex;
			std::string _pathname;

		public:
			std::mutex& GetMutex() { return _mutex; }

			void Write(CSZ txt, std::thread::id tid, CSZ filename, UINT32 line, Severity severity);

			static void FormatTime(char* buffer, size_t size);
			static CSZ GetSeverityLetter(Severity severity);
			static String ExtractFilename(CSZ filename);
		};
	}
}
