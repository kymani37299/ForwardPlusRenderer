#pragma once

#include <vector>
#include <memory>

#include "Render/RenderAPI.h"
#include "Utility/MemoryStrategies.h"
#include "Utility/Multithreading.h"

struct Resource;
struct Shader;

struct DescriptorAllocation
{
	bool Transient = false;
	RangeAllocation HeapAlloc;
};

class DescriptorHeapCPU
{
public:
	DescriptorHeapCPU(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors);
	D3D12_CPU_DESCRIPTOR_HANDLE Allocate();
	void Release(D3D12_CPU_DESCRIPTOR_HANDLE& handle);

	ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }

private:
	ComPtr<ID3D12DescriptorHeap> m_Heap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_HeapStart;

	ElementStrategy m_AllocStrategy;
	size_t m_ElementSize;
};

class DescriptorHeapGPU
{
public:
	DescriptorHeapGPU(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors, size_t numTransientDescriptors);

	DescriptorAllocation Allocate(size_t numDescriptors, bool transient = false);
	void Release(DescriptorAllocation& allocation);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(const DescriptorAllocation& alloc, size_t rangeIndex = 0);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(const DescriptorAllocation& alloc, size_t rangeIndex = 0);

	ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
private:
	ComPtr<ID3D12DescriptorHeap> m_Heap;

	D3D12_CPU_DESCRIPTOR_HANDLE m_HeapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE m_HeapStartGPU;

	D3D12_CPU_DESCRIPTOR_HANDLE m_HeapStartCPUTransient;
	D3D12_GPU_DESCRIPTOR_HANDLE m_HeapStartGPUTransient;

	RangeStrategy m_AllocStrategy;
	RingRangeStrategy m_AllocStrategyTransient;
	size_t m_ElementSize;
};

class DeferredTrash
{
private:
	struct DescriptorAllocationEntry
	{
		DescriptorHeapGPU* DescriptorHeap;
		DescriptorAllocation DescriptorAlloc;
	};

	struct DescriptorEntry
	{
		DescriptorHeapCPU* DescriptorHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor;
	};

	struct TrashBucket
	{
		std::vector<ComPtr<IUnknown>> DXResources;
		std::vector<DescriptorAllocationEntry> DescriptorAllocations;
		std::vector<DescriptorEntry> Descriptors;
		std::vector<Shader*> Shaders;
		std::vector<Resource*> Resources;
	};

private:
	static std::unordered_map<MTR::ThreadID, DeferredTrash*> s_Instances;

public:
	static DeferredTrash* Get()
	{
		MTR::ThreadID threadID = MTR::CurrentThreadID();
		if (!s_Instances.contains(threadID))
			s_Instances[threadID] = new DeferredTrash();
		return s_Instances[threadID];
	}

	void Put(IUnknown* resouce) { Buckets[CurrentBucket].DXResources.push_back(resouce); }
	void Put(DescriptorHeapGPU* heap, DescriptorAllocation descriptorAlloc) { Buckets[CurrentBucket].DescriptorAllocations.push_back({heap, descriptorAlloc }); }
	void Put(DescriptorHeapCPU* heap, D3D12_CPU_DESCRIPTOR_HANDLE handle) { Buckets[CurrentBucket].Descriptors.push_back({heap, handle }); }
	void Put(Shader* shader) { Buckets[CurrentBucket].Shaders.push_back(shader); }
	void Put(Resource* resource) { Buckets[CurrentBucket].Resources.push_back(resource); }

	void Clear();
	void ClearAll();
private:
	static constexpr uint32_t NUM_BUCKETS = 2;
	uint32_t CurrentBucket = 0;
	TrashBucket Buckets[NUM_BUCKETS];
};