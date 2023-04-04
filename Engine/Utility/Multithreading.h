#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <queue>
#include <sstream>
#include <chrono>

namespace MTR
{
	using ThreadID = std::thread::id;

	inline ThreadID CurrentThreadID() { return std::this_thread::get_id();  }
	inline bool IsThread(ThreadID id) { return id == CurrentThreadID(); }
	static std::string GetThreadName(ThreadID threadID) // TODO: More descriptive names
	{
		std::stringstream ss;
		ss << threadID;
		return ss.str();
	}

	inline void ThreadSleepMS(uint64_t duration)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(duration));
	}

	class Mutex
	{
	public:
		Mutex() { m_Mutex = new std::mutex{}; }
		~Mutex() { delete m_Mutex; }

		inline void Lock() { m_Mutex->lock(); }
		inline void Unlock() { m_Mutex->unlock(); }

	private:
		std::mutex* m_Mutex;
	};

	template<typename T>
	class MutexVector
	{
	public:
		inline void Add(const T& element)
		{
			m_Mutex.Lock();
			m_Data.push_back(element);
			m_Mutex.Unlock();
		}

		inline void AddAll(const std::vector<T>& elements)
		{
			m_Mutex.Lock();
			for (const T& element : elements) m_Data.push_back(element);
			m_Mutex.Unlock();
		}

		inline void Remove(size_t index)
		{
			m_Mutex.Lock();
			m_Data.erase(m_Data.begin() + index);
			m_Mutex.Unlock();
		}

		inline void Clear()
		{
			m_Mutex.Lock();
			m_Data.clear();
			m_Mutex.Unlock();
		}

		inline bool Empty()
		{
			m_Mutex.Lock();
			bool ret = m_Data.empty();
			m_Mutex.Unlock();
			return ret;
		}

		template<typename F>
		void ForEach(F&& f)
		{
			m_Mutex.Lock();
			for (T& e : m_Data) f(e);
			m_Mutex.Unlock();
		}

		template<typename F>
		void ForEach(F& f)
		{
			m_Mutex.Lock();
			for (T& e : m_Data) f(e);
			m_Mutex.Unlock();
		}

		template<typename F>
		void ForEachAndClear(F&& f)
		{
			m_Mutex.Lock();
			for (T& e : m_Data) f(e);
			m_Data.clear();
			m_Mutex.Unlock();
		}

		template<typename F>
		void ForEachAndClear(F& f)
		{
			m_Mutex.Lock();
			for (T& e : m_Data) f(e);
			m_Data.clear();
			m_Mutex.Unlock();
		}

		// TODO: Add [] override
		// TODO: Add foreach override

	private:
		Mutex m_Mutex;
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

		size_t Size() const
		{
			return m_Queue.size();
		}

	private:
		std::mutex              m_Mutex;
		std::condition_variable m_Condition;
		std::deque<T>           m_Queue;
	};
}