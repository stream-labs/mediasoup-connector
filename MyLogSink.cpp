#ifndef _DEBUG

#include "MyLogSink.h"

void MyLogSink::Start()
{
	rtc::LogMessage::AddLogToStream(this, rtc::WARNING);
}

void MyLogSink::Stop()
{
	rtc::LogMessage::RemoveLogToStream(this);
}

void MyLogSink::OnLogMessage(const std::string& message, rtc::LoggingSeverity severity)
{
	int obs_log_level = LOG_DEBUG;

	switch (severity)
	{
		case rtc::LoggingSeverity::LS_VERBOSE: obs_log_level = LOG_DEBUG; break;
		case rtc::LoggingSeverity::LS_INFO: obs_log_level = LOG_WARNING; break;
		case rtc::LoggingSeverity::LS_WARNING: obs_log_level = LOG_WARNING; break;
		case rtc::LoggingSeverity::LS_ERROR: obs_log_level = LOG_ERROR; break;
	}

	blog(obs_log_level, std::string("webrtc: " + message).c_str());
}

void MyLogSink::OnLogMessage(const std::string& message)
{

}

#endif
