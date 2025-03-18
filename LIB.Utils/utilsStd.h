#pragma once

#include <vector>

namespace utils
{
namespace std23
{

template<typename T>
class vector : public std::vector<T>
{
public:
	vector() = default;
	vector(const std::vector<T>& val) :std::vector<T>(val) {}

	void append_range(const std::vector<T>& range)
	{
		this->insert(this->end(), range.cbegin(), range.cend());
	}

	void insert_range(std::vector<T>::const_iterator pos, const std::vector<T>& range)
	{
		this->insert(pos, range.cbegin(), range.cend());
	}
};

}
}
