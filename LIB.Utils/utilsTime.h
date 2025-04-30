#pragma once

#include <chrono>
#include <string>

#include <time.h>

namespace utils
{
namespace time
{

class tDateTime
{
	std::time_t Value = 0;

public:
	tDateTime() = default;
	tDateTime(time_t val) :Value(val) {} // it is not explicit

	static tDateTime Now();

	static tDateTime Parse(const std::string& value, const std::string& format);
	static tDateTime Parse(const std::string& value);
	static tDateTime ParsePath(const std::string& value);

	tm GetTM() const;

	std::string ToString(const std::string& format) const;
	std::string ToString() const;
	std::string ToStringPath() const;
};

}
}
