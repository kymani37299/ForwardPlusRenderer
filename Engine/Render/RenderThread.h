#pragma once

#include "Common.h"
#include "Utility/Multithreading.h"

struct GraphicsContext;

// TODO: Add RenderTask priority
class RenderTask
{
public:
	virtual ~RenderTask() {}
	virtual void Run(GraphicsContext& context) = 0;

	inline bool ShouldStop() const { return !m_Running; }
	inline bool IsRunning() const { return m_Running; }
	inline void SetRunning(bool running) { m_Running = running; }

private:
	bool m_Running = false;
};

class RenderTaskKiller : public RenderTask
{
public:
	void Run(GraphicsContext&) override {}
};

class RenderThread
{
public:
	RenderThread();
	~RenderThread();

public:
	void Submit(RenderTask* task);
	void Run();
	void ResetAndWait();
	void Stop();

	uint32_t RemainingTaskCount() const
	{
		return m_TaskQueue.Size();
	}

private:
	RenderTaskKiller m_ThreadKiller;
	MTR::BlockingQueue<RenderTask*> m_TaskQueue;
	std::thread* m_ThreadHandle = nullptr;
	std::atomic<RenderTask*> m_CurrentTask = nullptr;
	bool m_Running = false;
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
	void Submit(RenderTask* task);

private:
	uint32_t m_NumThreads;
	std::vector<RenderThread*> m_LoadingThreads;
};