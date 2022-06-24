#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <sstream>

#define CURRENT_THREAD std::this_thread::get_id()

namespace MTR
{
	using ThreadID = std::thread::id;

	inline bool IsThread(ThreadID id) { return id == CURRENT_THREAD; }
	static std::string GetThreadName(ThreadID threadID) // TODO: More descriptive names
	{
		std::stringstream ss;
		ss << threadID;
		return ss.str();
	}

	template<typename T>
	class MutexVector
	{
	public:
		MutexVector() { m_Mutex = new std::mutex(); }
		~MutexVector() { delete m_Mutex; }

		inline void Lock() { m_Mutex->lock(); }
		inline void Unlock() { m_Mutex->unlock(); }

		inline void Add(const T& element)
		{
			Lock();
			m_Data.push_back(element);
			Unlock();
		}

		inline void AddAll(const std::vector<T>& elements)
		{
			Lock();
			for (const T& element : elements) m_Data.push_back(element);
			Unlock();
		}

		inline void Remove(size_t index)
		{
			Lock();
			m_Data.erase(m_Data.begin() + index);
			Unlock();
		}

		inline void Clear()
		{
			Lock();
			m_Data.clear();
			Unlock();
		}

		inline bool Empty()
		{
			Lock();
			bool ret = m_Data.empty();
			Unlock();
			return ret;
		}

		template<typename F>
		void ForEach(F& f)
		{
			Lock();
			for (T& e : m_Data) f(e);
			Unlock();
		}

		template<typename F>
		void ForEachAndClear(F& f)
		{
			Lock();
			for (T& e : m_Data) f(e);
			m_Data.clear();
			Unlock();
		}

		// TODO: Add [] override
		// TODO: Add foreach override

	private:
		std::mutex* m_Mutex;
		std::vector<T> m_Data;
	};

	template <typename T>
	class BlockingQueue
	{
	public:
		void Push(const T& value)
		{
			{
				std::unique_lock<std::mutex> lock(m_Mutex);
				m_Queue.push_front(value);
			}
			m_Condition.notify_one();
		}

		T Pop()
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Condition.wait(lock, [=] { return !m_Queue.empty(); });
			T rc(std::move(m_Queue.back()));
			m_Queue.pop_back();
			return rc;
		}

		void Clear()
		{
			std::unique_lock<std::mutex> lock(m_Mutex);
			m_Queue.clear();
		}

		uint32_t Size() const
		{
			return m_Queue.size();
		}

	private:
		std::mutex              m_Mutex;
		std::condition_variable m_Condition;
		std::deque<T>           m_Queue;
	};
}