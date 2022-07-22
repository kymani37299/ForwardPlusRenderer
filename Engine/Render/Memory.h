#pragma once

#include <vector>
#include <memory>

#include "Render/RenderAPI.h"
#include "Utility/MemoryStrategies.h"

struct Resource;
struct Shader;

using CPUAllocStrategy = ElementStrategy<size_t>;
using GPUAllocStrategy = PageStrategy<uint64_t>;

struct DescriptorHeapCPU
{
	DescriptorHeapCPU(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors);
	D3D12_CPU_DESCRIPTOR_HANDLE Alloc();
	void Release(D3D12_CPU_DESCRIPTOR_HANDLE& handle);

	ComPtr<ID3D12DescriptorHeap> Heap;
	D3D12_CPU_DESCRIPTOR_HANDLE HeapStart;

	CPUAllocStrategy AllocStrategy;
	size_t ElementSize;
};

struct DescriptorHeapGPU
{
	struct Allocation
	{
		D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle;
		D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle;
	};

	DescriptorHeapGPU(D3D12_DESCRIPTOR_HEAP_TYPE type, uint64_t numPages, uint64_t numDescriptors);

	GPUAllocStrategy::Page NewPage();
	void ReleasePage(GPUAllocStrategy::Page& page);
	Allocation Alloc(GPUAllocStrategy::Page& page);

	ComPtr<ID3D12DescriptorHeap> Heap;
	D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCPU;
	D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGPU;

	GPUAllocStrategy AllocStrategy;
	size_t ElementSize;
	uint32_t PageSize;
};

class DeferredTrash
{
private:
	struct DescriptorPagesEntry
	{
		DescriptorHeapGPU* DescriptorHeap;
		GPUAllocStrategy::Page Page;
	};

	struct DescriptorEntry
	{
		DescriptorHeapCPU* DescriptorHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE Descriptor;
	};

	struct TrashBucket
	{
		std::vector<ComPtr<IUnknown>> DXResources;
		std::vector<DescriptorPagesEntry> DescriptorPages;
		std::vector<DescriptorEntry> Descriptors;
		std::vector<Shader*> Shaders;
		std::vector<Resource*> Resources;
	};
public:
	static void Put(IUnknown* resouce) { Buckets[CurrentBucket].DXResources.push_back(resouce); }
	static void Put(DescriptorHeapGPU* heap, GPUAllocStrategy::Page page) { Buckets[CurrentBucket].DescriptorPages.push_back({heap, page}); }
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