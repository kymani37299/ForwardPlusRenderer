#pragma once

#include "Common.h"
#include "Utility/Multithreading.h"

struct GraphicsContext;

enum class RenderTaskPriority
{
	High,
	Medium,
	Low,
};

class RenderTask
{
public:
	virtual ~RenderTask() {}
	virtual void Run(GraphicsContext& context) = 0;

	bool ShouldStop() const { return !m_Running; }
	bool IsRunning() const { return m_Running; }
	void SetRunning(bool running) { m_Running = running; }

	void SetPriority(RenderTaskPriority priority) { m_Priority = priority; }
	RenderTaskPriority GetPriority() const { return m_Priority; }

private:
	RenderTaskPriority m_Priority = RenderTaskPriority::Medium;
	bool m_Running = false;
};

enum class RenderThreadState
{
	WaitingForTask,
	RunningTask,
	Paused,
	Stopped,
};

class RenderThread
{
public:
	RenderThread();
	~RenderThread();

public:
	void Submit(RenderTask* task);
	void Run();

	void SetWantedState(RenderThreadState wantedState);
	RenderThreadState GetState() const { return m_State; }

private:
	MTR::MutexVector<RenderTask*> m_TaskQueue;
	std::thread* m_ThreadHandle = nullptr;
	std::atomic<RenderTask*> m_CurrentTask = nullptr;
	
	RenderThreadState m_WantedState = RenderThreadState::RunningTask;
	RenderThreadState m_State = RenderThreadState::Stopped;

	uint32_t m_ThreadLocalQueueIndex = 0;
	std::vector<RenderTask*> m_ThreadLocalQueue;
};

class RenderThreadPool
{
public:
	static void Init(uint32_t numThreads) { s_Instance = new RenderThreadPool(numThreads); }
	static RenderThreadPool* Get() { return s_Instance; }
	static void Destroy() { SAFE_DELETE(s_Instance); }

private:
	static RenderThreadPool* s_Instance;

	RenderThreadPool(uint32_t numThreads);
	~RenderThreadPool();

public:
	void FlushAndPauseExecution();
	void ResumeExecution();

	void Submit(RenderTask* task);

private:
	uint32_t m_NumThreads;
	std::vector<RenderThread*> m_LoadingThreads;
};