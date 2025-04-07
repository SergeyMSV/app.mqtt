#pragma once

#include <vector>

#ifdef __cpp_lib_containers_ranges
namespace std23 = std;
#else // __cpp_lib_containers_ranges
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
	vector(std::vector<T>&& val) :std::vector<T>(std::move(val)) {}

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

#endif // __cpp_lib_containers_ranges
