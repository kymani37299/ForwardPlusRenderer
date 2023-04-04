#pragma once

#include <vector>
#include <memory>

#include "Render/RenderAPI.h"
#include "Utility/MemoryStrategies.h"
#include "Utility/Multithreading.h"

class DescriptorHeap;

class DescriptorAllocation
{
public:
	DescriptorAllocation() = default;

	DescriptorAllocation(bool transient, RangeAllocation alloc, DescriptorHeap* owner) :
		m_Transient(transient),
		m_Alloc(alloc),
		m_Owner(owner) 
	{
		Init();
	}

	bool IsValid() const
	{
		return m_Alloc.Start != INVALID_ALLOCATION && m_Owner != nullptr;
	}

	void Init();
	void Release();

	// TODO: Create some kind of CopyTo function

	size_t GetDescriptorCount() const
	{
		return m_Alloc.NumElements;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(size_t rangeIndex) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(size_t rangeIndex) const;

	const D3D12_GPU_DESCRIPTOR_HANDLE& GetGPUHandle() const
	{
		return m_FirstGPUHandle;
	}

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetCPUHandle() const
	{
		return m_FirstCPUHandle;
	}

private:
	bool m_Transient = false;
	RangeAllocation m_Alloc;
	DescriptorHeap* m_Owner = nullptr;

	D3D12_GPU_DESCRIPTOR_HANDLE m_FirstGPUHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_FirstCPUHandle;
};

class DescriptorHeap
{
	friend class DescriptorAllocation;
public:
	DescriptorHeap(bool gpuVisible, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors, size_t numTransientDescriptors = 0);

	void Initialize(size_t numDescriptors, size_t numTransientDescriptors = 0);

	DescriptorAllocation Allocate(size_t numDescriptors = 1);
	DescriptorAllocation AllocateTransient(size_t numDescriptors = 1);
	
	void Release(DescriptorAllocation& allocation);

	ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
	
	// Hack
	MTR::Mutex& GetAllocationLock() { return m_AllocationLock; }

private:
	bool m_GpuVisible;

	RangeStrategy m_AllocStrategy;
	RingRangeStrategy m_AllocStrategyTransient;
	size_t m_ElementSize;

	ComPtr<ID3D12DescriptorHeap> m_Heap;

	D3D12_CPU_DESCRIPTOR_HANDLE m_HeapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_HeapStartGPU;

	D3D12_CPU_DESCRIPTOR_HANDLE m_HeapStartCPUTransient;
	D3D12_GPU_DESCRIPTOR_HANDLE m_HeapStartGPUTransient;

	MTR::Mutex m_AllocationLock;
};