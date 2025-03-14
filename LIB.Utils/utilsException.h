///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsBase.h
// 2014-09-24
// Standard ISO/IEC 114882, C++17
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace utils
{

inline std::string GetLogMessage(const std::string& msg, const std::string& filename, int line)
{
	const std::filesystem::path Path(filename);
	return msg + " " + Path.filename().string() + ":" + std::to_string(line);
}

}

#define THROW_INVALID_ARGUMENT(msg) throw std::invalid_argument{ utils::GetLogMessage(msg, __FILE__, __LINE__) }
#define THROW_RUNTIME_ERROR(msg) throw std::runtime_error{ utils::GetLogMessage(msg, __FILE__, __LINE__) }
