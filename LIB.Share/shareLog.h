#pragma once

#include <libConfig.h>

#include <iostream>

#include <utilsChrono.h>
#include <utilsLog.h>

namespace share
{

class tLogger : public utils::log::tLog
{
public:
	tLogger() = default;

	void PacketSent(const std::string& msg, const std::vector<std::uint8_t>& data)
	{
		WriteLine(true, msg, utils::log::tColor::LightBlue);
		WriteHex(true, "SND", data, utils::log::tColor::Blue);
	}

	void PacketReceivedRaw(const std::vector<std::uint8_t>& data)
	{
		WriteHex(true, "RCV", data, utils::log::tColor::Green);
	}

	void PacketReceived(const std::string& msg)
	{
		WriteLine(true, msg, utils::log::tColor::LightGreen);
	}

	void MeasureDuration(const std::string& msg)
	{
		WriteLine(true, msg, utils::log::tColor::LightYellow);
	}

	void PublishMessage(const std::string& topicName, const std::vector<std::uint8_t>& payload)
	{
		WriteHex(true, topicName, payload, utils::log::tColor::LightMagenta);
	}

	void Operation(const std::string& msg)
	{
		WriteLine(true, msg, utils::log::tColor::Yellow);
	}

	void Exception(const std::string& msg)
	{
		WriteLine(true, msg, utils::log::tColor::Red);
	}

	void Trace(const std::string& msg)
	{
		WriteLine(true, msg, utils::log::tColor::LightCyan);
	}

	void TestMessage(const std::string& msg)
	{
		WriteLine(true, msg, utils::log::tColor::White);
	}

protected:
	void WriteLog(const std::string& msg) override final
	{
		std::cout << msg;
	}
};

}

extern share::tLogger g_Log;

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
