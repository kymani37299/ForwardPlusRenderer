#include "RenderThread.h"

#include "Render/Context.h"
#include "Render/Commands.h"

RenderThreadPool* RenderThreadPool::s_Instance = nullptr;

RenderThread::RenderThread()
{
	m_ThreadHandle = new std::thread(&RenderThread::Run, this);
}

RenderThread::~RenderThread()
{
	SetWantedState(RenderThreadState::Stopped);
	m_ThreadHandle->join();
	delete m_ThreadHandle;

	m_TaskQueue.ForEachAndClear([](RenderTask* task) {
		delete task;
		});
}

void RenderThread::Submit(RenderTask* task)
{
	m_TaskQueue.Add(task);
}

bool RenderTaskComparator(const RenderTask* l, const RenderTask* r)
{
	const uint32_t lPrio = l ? EnumToInt(l->GetPriority()) : 1000;
	const uint32_t rPrio = r ? EnumToInt(r->GetPriority()) : 1000;
	return lPrio < rPrio;
}

void RenderThread::Run()
{
	OPTICK_THREAD("RenderThread");

	GraphicsContext& context = ContextManager::Get().CreateWorkerContext();
	while (m_WantedState != RenderThreadState::Stopped)
	{
		while (m_WantedState == RenderThreadState::Paused)
		{
			m_State = RenderThreadState::Paused;
			MTR::ThreadSleepMS(100);
		}

		// Fill local queue if we already processed everything
		if (m_ThreadLocalQueueIndex >= m_ThreadLocalQueue.size())
		{
			m_ThreadLocalQueue.clear();

			m_State = RenderThreadState::WaitingForTask;
			while (m_TaskQueue.Empty()) MTR::ThreadSleepMS(200);

			m_ThreadLocalQueueIndex = 0;
			m_TaskQueue.ForEachAndClear([this](RenderTask* task) {
				m_ThreadLocalQueue.push_back(task);
				});

			std::sort(m_ThreadLocalQueue.begin(), m_ThreadLocalQueue.end(), RenderTaskComparator);
		}

		// Get next task
		m_CurrentTask = m_ThreadLocalQueue[m_ThreadLocalQueueIndex++];
		if (m_CurrentTask == nullptr) continue;

		// Execute task
		GFX::Cmd::BeginRecording(context);
		m_State = RenderThreadState::RunningTask;
		m_CurrentTask.load()->SetRunning(true);
		m_CurrentTask.load()->Run(context);
		m_CurrentTask.load()->SetRunning(false);
		GFX::Cmd::EndRecordingAndSubmit(context);

		RenderTask* lastTask = m_CurrentTask.exchange(nullptr);
		delete lastTask;
	}
	m_State = RenderThreadState::Stopped;
}

void RenderThread::SetWantedState(RenderThreadState wantedState)
{
	m_WantedState = wantedState;
	Submit(nullptr);
	if (m_WantedState == RenderThreadState::Stopped)
	{
		RenderTask* currentTask = m_CurrentTask;
		if (currentTask) currentTask->SetRunning(false);
	}
}

RenderThreadPool::RenderThreadPool(uint32_t numThreads)
{
	m_NumThreads = numThreads;
	m_LoadingThreads.resize(m_NumThreads);
	for (uint32_t i = 0; i < m_NumThreads; i++)
	{
		m_LoadingThreads[i] = new RenderThread();
	}
}

RenderThreadPool::~RenderThreadPool()
{
	for (uint32_t i = 0; i < m_NumThreads; i++)
	{
		delete m_LoadingThreads[i];
	}
}

void RenderThreadPool::FlushAndPauseExecution()
{
	// Set all threads to paused
	for (RenderThread* rt : m_LoadingThreads)
	{
		rt->SetWantedState(RenderThreadState::Paused);
	}

	// Wait for all threads to finish execution
	for (RenderThread* rt : m_LoadingThreads)
	{
		while(rt->GetState() != RenderThreadState::Paused)
		{
			MTR::ThreadSleepMS(20);
		}
	}
}

void RenderThreadPool::ResumeExecution()
{
	for (RenderThread* rt : m_LoadingThreads)
	{
		rt->SetWantedState(RenderThreadState::RunningTask);
	}
}

void RenderThreadPool::Submit(RenderTask* task)
{
	const uint32_t randomThread = rand() % m_NumThreads;
	m_LoadingThreads[randomThread]->Submit(task);
}