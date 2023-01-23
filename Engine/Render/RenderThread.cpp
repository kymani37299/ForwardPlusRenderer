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
	Stop();
	m_ThreadHandle->join();
	delete m_ThreadHandle;
}

void RenderThread::Submit(RenderTask* task)
{
	m_TaskQueue.Push(task);
}

void RenderThread::Run()
{
	m_Running = true;
	ScopedRef<GraphicsContext> context = ScopedRef<GraphicsContext>(GFX::Cmd::CreateGraphicsContext());
	while (m_Running)
	{
		m_CurrentTask = m_TaskQueue.Pop();
		if (m_CurrentTask.load() == &m_ThreadKiller) break;
		m_CurrentTask.load()->SetRunning(true);
		m_CurrentTask.load()->Run(*context);
		m_CurrentTask.load()->SetRunning(false);

		// TODO: In flight tasks
		GFX::Cmd::SubmitContext(*context);
		GFX::Cmd::FlushContext(*context);
		GFX::Cmd::ResetContext(*context.get());
		
		RenderTask* lastTask = m_CurrentTask.exchange(nullptr);
		delete lastTask;
	}
}

void RenderThread::ResetAndWait()
{
	// 1. Stop loading thread
	Stop();

	// 2. Wait it to finish
	m_ThreadHandle->join();

	// 3. Restart loading thread
	delete m_ThreadHandle;
	m_Running = true;
	m_TaskQueue.Clear();
	m_ThreadHandle = new std::thread(&RenderThread::Run, this);
}

void RenderThread::Stop()
{
	m_TaskQueue.Clear();
	m_TaskQueue.Push(&m_ThreadKiller);
	RenderTask* currentTask = m_CurrentTask;
	if (currentTask) currentTask->SetRunning(false);
	m_Running = false;
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

void RenderThreadPool::Submit(RenderTask* task)
{
	const uint32_t randomThread = rand() % m_NumThreads;
	m_LoadingThreads[randomThread]->Submit(task);
}