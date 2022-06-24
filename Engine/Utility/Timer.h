#pragma once

#include <chrono>

class Timer
{
public:
	void Start()
	{
		m_Running = true;
		m_BeginTime = std::chrono::high_resolution_clock::now();
	}

	void Stop()
	{
		m_Running = false;
		m_EndTime = std::chrono::high_resolution_clock::now();
	}

	inline float GetTimeMS() const
	{
		const std::chrono::time_point<std::chrono::steady_clock> endTime = m_Running ? std::chrono::high_resolution_clock::now() : m_EndTime;
		return std::chrono::duration<float, std::milli>(endTime - m_BeginTime).count();
	}

private:
	bool m_Running = false;
	std::chrono::time_point<std::chrono::steady_clock> m_BeginTime = std::chrono::high_resolution_clock::now();
	std::chrono::time_point<std::chrono::steady_clock> m_EndTime = std::chrono::high_resolution_clock::now();
};