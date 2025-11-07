///////////////////////////////////////////////////////////////////////////////////////////////////
// utilsMultithread
// 2024-04-16
// C++14
///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <deque>
#include <mutex>

namespace utils
{
namespace multithread
{

template <class T, std::size_t Size>
class tQueue
{
	std::deque<T> m_Queue;
	mutable std::mutex m_QueueMtx;

public:
	T get_front()
	{
		std::lock_guard<std::mutex> guard(m_QueueMtx);
		if (m_Queue.empty())
			return {};
		T Pack = m_Queue.front();
		m_Queue.pop_front();
		return Pack;
	}
	void push_back(const T& val)
	{
		std::lock_guard<std::mutex> guard(m_QueueMtx);
		if (m_Queue.size() >= Size)
			m_Queue.pop_front();
		m_Queue.push_back(val);
	}
	void push_back(T&& val)
	{
		std::lock_guard<std::mutex> guard(m_QueueMtx);
		if (m_Queue.size() >= Size)
			m_Queue.pop_front();
		m_Queue.push_back(std::move(val));
	}
	void clear()
	{
		std::lock_guard<std::mutex> guard(m_QueueMtx);
		m_Queue.clear();
	}
	bool empty() const
	{
		std::lock_guard<std::mutex> guard(m_QueueMtx);
		return m_Queue.empty();
	}
};

}
}
