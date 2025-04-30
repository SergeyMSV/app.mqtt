///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsLog
// 2016-05-16 (before)
// Standard ISO/IEC 114882, C++11
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <libConfig.h>

#include <cstdint>

#include <mutex>
#include <string>
#include <vector>

namespace utils
{
namespace log
{

enum class tColor
{
	Black,
	Red,
	Green,
	Yellow,
	Blue,
	Magenta,
	Cyan,
	White,
	Default,
	LightGray,
	LightRed,
	LightGreen,
	LightYellow,
	LightBlue,
	LightMagenta,
	LightCyan,
	LightWhite,
};

#ifdef LIB_UTILS_LOG

class tLog
{
	mutable std::mutex m_Mtx;

public:
	tLog() = default;
	virtual ~tLog() { }

	void Write(bool timestamp, tColor colorText, const std::string& msg);

	void WriteLine();
	void WriteLine(bool timestamp, tColor colorText, const std::string& msg);

	void WriteHex(bool timestamp, tColor colorText, const std::string& msg, const std::vector<std::uint8_t>& data);

protected:
	virtual std::string GetLabel() const { return {}; }

	virtual void WriteLog(const std::string& msg) = 0;

private:
	virtual void WriteLog(bool timestamp, bool endl, tColor colorText, const std::string& msg);
};

#else // LIB_UTILS_LOG

class tLog
{
public:
	tLog() = default;
	virtual ~tLog() { }

	void Write(bool timestamp, tColor colorText, const std::string& msg) { }

	void WriteLine() { }
	void WriteLine(bool timestamp, tColor colorText, const std::string& msg) { }

	void WriteHex(bool timestamp, tColor colorText, const std::string& msg, const std::vector<std::uint8_t>& data) { }

protected:
	virtual std::string GetLabel() const { return {}; }

	virtual void WriteLog(const std::string& msg) = 0;
};

#endif // LIB_UTILS_LOG

}
}
