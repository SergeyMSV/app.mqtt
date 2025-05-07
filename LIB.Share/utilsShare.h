#pragma once

#include <libConfig.h>

#include <iostream>

#include <utilsChrono.h>
#include <utilslog.h>

namespace utils
{
namespace share
{

class tLogger : public utils::log::tLog
{
public:
	tLogger() = default;

	void PacketSent(const std::string& msg, const std::vector<std::uint8_t>& data)
	{
		WriteLine(true, utils::log::tColor::LightBlue, msg);
		WriteHex(true, utils::log::tColor::Blue, "SND", data);
	}

	void PacketReceivedRaw(const std::vector<std::uint8_t>& data)
	{
		WriteHex(true, utils::log::tColor::Green, "RCV", data);
	}

	void PacketReceived(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::LightGreen, msg);
	}

	void MeasureDuration(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::LightYellow, msg);
	}

	void Operation(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::Yellow, msg);
	}

	void Exception(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::Red, msg);
	}

	void Trace(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::LightCyan, msg);
	}

	void TestMessage(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::White, msg);
	}

protected:
	void WriteLog(const std::string& msg) override final
	{
		std::cout << msg;
	}
};

}
}

extern utils::share::tLogger g_Log;

namespace utils
{
namespace share
{

class tMeasureDuration : public utils::chrono::tTimeDuration
{
	std::string m_Label;

public:
	explicit tMeasureDuration(const std::string& label)
		:utils::chrono::tTimeDuration(), m_Label(label)
	{
		if (!m_Label.empty())
			m_Label += ": ";
	}

	~tMeasureDuration()
	{
		const std::string Msg = m_Label + std::to_string(Get<utils::chrono::ttime_ms>()) + " ms";

		g_Log.MeasureDuration(Msg);
	}
};

}
}
