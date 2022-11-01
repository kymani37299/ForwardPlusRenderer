#pragma once

#include <vector>
#include <memory>

#include "Render/RenderAPI.h"
#include "Utility/MemoryStrategies.h"

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
public:
	static void Put(IUnknown* resouce) { Buckets[CurrentBucket].DXResources.push_back(resouce); }
	static void Put(DescriptorHeapGPU* heap, DescriptorAllocation descriptorAlloc) { Buckets[CurrentBucket].DescriptorAllocations.push_back({heap, descriptorAlloc }); }
	static void Put(DescriptorHeapCPU* heap, D3D12_CPU_DESCRIPTOR_HANDLE handle) { Buckets[CurrentBucket].Descriptors.push_back({heap, handle }); }
	static void Put(Shader* shader) { Buckets[CurrentBucket].Shaders.push_back(shader); }
	static void Put(Resource* resource) { Buckets[CurrentBucket].Resources.push_back(resource); }

	static void Clear();
	static void ClearAll();
private:
	static constexpr uint32_t NUM_BUCKETS = 2;
	static uint32_t CurrentBucket;
	static TrashBucket Buckets[NUM_BUCKETS];
};