#pragma once

#include "Utility/Multithreading.h"

struct ID3D11DeviceContext1;

class LoadingTask
{
public:
    virtual ~LoadingTask() {}
    virtual void Run(ID3D11DeviceContext1* context) = 0;

    inline bool ShouldStop() const { return !m_Running; }
    inline bool IsRunning() const { return m_Running; }
    inline void SetRunning(bool running) { m_Running = running; }

private:
    bool m_Running = false;
};

class PoisonPillTask : public LoadingTask
{
    static PoisonPillTask* s_Instance;
public:
    static PoisonPillTask* Get() { if (!s_Instance) s_Instance = new PoisonPillTask(); return s_Instance; }
    void Run(ID3D11DeviceContext1*) override {}
private:
    PoisonPillTask() {}
};

class LoadingThread
{
public:
    static void Init() { s_Instance = new LoadingThread(); }
    static LoadingThread* Get() { return s_Instance; }
    static void Destroy() { SAFE_DELETE(s_Instance); }

private:
    static LoadingThread* s_Instance;

    LoadingThread()
    {
        m_ThreadHandle = new std::thread(&LoadingThread::Run, this);
    }

    ~LoadingThread()
    {
        Stop();
        m_ThreadHandle->join();
        delete m_ThreadHandle;
    }

public:
    void Submit(LoadingTask* task)
    {
        m_TaskQueue.Push(task);
    }

    void Run()
    {
        m_Running = true;
        ID3D11DeviceContext1* context = nullptr; // TODO: Create deferred context
        while (m_Running)
        {
            m_CurrentTask = m_TaskQueue.Pop();
            if (m_CurrentTask.load() == PoisonPillTask::Get()) break;
            m_CurrentTask.load()->SetRunning(true);
            m_CurrentTask.load()->Run(context);
            // TODO: Submit context
            m_CurrentTask.load()->SetRunning(false);

            LoadingTask* lastTask = m_CurrentTask.exchange(nullptr);
            delete lastTask;
        }
        // TODO: Release context
    }

    void ResetAndWait()
    {
        // 1. Stop loading thread
        Stop();

        // 2. Wait it to finish
        m_ThreadHandle->join();

        // 3. Restart loading thread
        delete m_ThreadHandle;
        m_Running = true;
        m_TaskQueue.Clear();
        m_ThreadHandle = new std::thread(&LoadingThread::Run, this);
    }

    void Stop()
    {
        m_TaskQueue.Clear();
        m_TaskQueue.Push(PoisonPillTask::Get());
        LoadingTask* currentTask = m_CurrentTask;
        if (currentTask) currentTask->SetRunning(false);
        m_Running = false;
    }

private:
    MTR::BlockingQueue<LoadingTask*> m_TaskQueue;
    std::thread* m_ThreadHandle = nullptr;
    std::atomic<LoadingTask*> m_CurrentTask = nullptr;
    bool m_Running = false;
};