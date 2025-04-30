#include "utilsTime.h"

#include <iomanip>
#include <sstream>

namespace utils
{
namespace time
{

constexpr char g_format_datetime[] = "%Y-%m-%d %H:%M:%S"; // "%F %T"; - date & time are not parsed with "%F %T" (2024-11-12 14:53:26)
constexpr char g_format_path[] = "%Y-%m-%d_%H-%M-%S";

tDateTime tDateTime::Now()
{
	const auto TimeNow = std::chrono::system_clock::now();
	return tDateTime{ std::chrono::system_clock::to_time_t(TimeNow) };
}

tDateTime tDateTime::Parse(const std::string& value, const std::string& format)
{
	tm DateTime{};
	std::istringstream iss(value);
	iss >> std::get_time(&DateTime, format.c_str());
	return tDateTime{ mktime(&DateTime) };
}

tDateTime tDateTime::Parse(const std::string& value)
{
	return Parse(value, g_format_datetime);
}

tDateTime tDateTime::ParsePath(const std::string& value)
{
	return Parse(value, g_format_path);
}

tm tDateTime::GetTM() const
{
	tm TmBuf;
#ifdef _WIN32
	localtime_s(&TmBuf, &Value);
#else // _WIN32
	localtime_r(&Value, &TmBuf);
#endif // _WIN32
	return TmBuf;
}

std::string tDateTime::ToString(const std::string& format) const
{
	tm TmBuf = GetTM();
	std::ostringstream oss;
	oss << std::put_time(&TmBuf, format.c_str());
	return oss.str();
}

std::string tDateTime::ToString() const
{
	return ToString(g_format_datetime);
}

std::string tDateTime::ToStringPath() const
{
	return ToString(g_format_path);
}

}
}