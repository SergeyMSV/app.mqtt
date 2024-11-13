#pragma once

#include <iostream>

#include <utilsChrono.h>
#include <utilslog.h>

namespace test
{

class tLogger : public utils::log::tLog
{
public:
	tLogger() = default;

	void PacketSent(const std::string& msg)
	{
		WriteLine(true, utils::log::tColor::LightBlue, msg);
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

extern test::tLogger g_Log;

namespace test
{

class tMeasureDuration : public utils::tTimeDuration
{
	std::string m_Label;

public:
	explicit tMeasureDuration(const std::string& label)
		:utils::tTimeDuration(), m_Label(label)
	{
		if (!m_Label.empty())
			m_Label += ": ";
	}

	~tMeasureDuration()
	{
		const std::string Msg = m_Label + std::to_string(Get<utils::ttime_ms>()) + " ms";

		g_Log.MeasureDuration(Msg);
	}
};

}

struct tMeasure
{
};
