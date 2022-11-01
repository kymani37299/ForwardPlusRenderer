#include "Memory.h"

#include "Render/Device.h"
#include "Render/Shader.h"
#include "Render/Resource.h"

ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible, size_t numDescriptors, size_t& incrementSize)
{
	ID3D12DescriptorHeap* heap = nullptr;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	heapDesc.NumDescriptors = (UINT) numDescriptors;
	heapDesc.Type = type;
	heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	heapDesc.NodeMask = 0;
	API_CALL(Device::Get()->GetHandle()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&heap)));

	incrementSize = Device::Get()->GetHandle()->GetDescriptorHandleIncrementSize(type);

	return heap;
}

DescriptorHeapCPU::DescriptorHeapCPU(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors) :
	m_AllocStrategy(numDescriptors)
{
	m_Heap = CreateDescriptorHeap(type, false, numDescriptors, m_ElementSize);
	m_HeapStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapCPU::Allocate()
{
	size_t index = m_AllocStrategy.Allocate();
	ASSERT(index != INVALID_ALLOCATION, "DescriptorHeapCPU memory overflow!");

	D3D12_CPU_DESCRIPTOR_HANDLE descriptor = m_HeapStart;
	descriptor.ptr += index * m_ElementSize;
	return descriptor;
}

void DescriptorHeapCPU::Release(D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
	size_t index = handle.ptr;
	index -= m_HeapStart.ptr;
	index /= m_ElementSize;
	m_AllocStrategy.Release(index);

	handle.ptr = INVALID_ALLOCATION;
}

DescriptorHeapGPU::DescriptorHeapGPU(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors, size_t numTransientDescriptors) :
	m_AllocStrategy(numDescriptors),
	m_AllocStrategyTransient(numTransientDescriptors)
{
	m_Heap = CreateDescriptorHeap(type, true, numDescriptors + numTransientDescriptors, m_ElementSize);
	m_HeapStartCPU = m_Heap->GetCPUDescriptorHandleForHeapStart();
	m_HeapStartGPU = m_Heap->GetGPUDescriptorHandleForHeapStart();

	m_HeapStartCPUTransient.ptr = m_HeapStartCPU.ptr + numDescriptors * m_ElementSize;
	m_HeapStartGPUTransient.ptr = m_HeapStartGPU.ptr + numDescriptors * m_ElementSize;
}

DescriptorAllocation DescriptorHeapGPU::Allocate(size_t numDescriptors, bool transient)
{
	DescriptorAllocation alloc{};
	alloc.HeapAlloc = transient ? m_AllocStrategyTransient.Allocate(numDescriptors) : m_AllocStrategy.Allocate(numDescriptors);
	alloc.Transient = transient;
	ASSERT(alloc.HeapAlloc.Start != INVALID_ALLOCATION, "DescriptorHeapGPU memory overflow!");
	return alloc;
}

void DescriptorHeapGPU::Release(DescriptorAllocation& allocation)
{
	ASSERT(allocation.HeapAlloc.Start != INVALID_ALLOCATION, "Cannot release invalid allocation!");
	if (allocation.Transient)
		m_AllocStrategyTransient.Release(allocation.HeapAlloc);
	else
		m_AllocStrategy.Release(allocation.HeapAlloc);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapGPU::GetGPUHandle(const DescriptorAllocation& alloc, size_t rangeIndex)
{
	ASSERT(rangeIndex < alloc.HeapAlloc.NumElements, "DescriptorHeapGPU memory overflow!");
	D3D12_GPU_DESCRIPTOR_HANDLE descriptor = alloc.Transient ? m_HeapStartGPUTransient : m_HeapStartGPU;
	descriptor.ptr += (alloc.HeapAlloc.Start + rangeIndex) * m_ElementSize;
	return descriptor;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapGPU::GetCPUHandle(const DescriptorAllocation& alloc, size_t rangeIndex)
{
	ASSERT(rangeIndex < alloc.HeapAlloc.NumElements, "DescriptorHeapGPU memory overflow!");
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor = alloc.Transient ? m_HeapStartCPUTransient : m_HeapStartCPU;
	descriptor.ptr += (alloc.HeapAlloc.Start + rangeIndex) * m_ElementSize;
	return descriptor;
}

uint32_t DeferredTrash::CurrentBucket = 0;
DeferredTrash::TrashBucket DeferredTrash::Buckets[DeferredTrash::NUM_BUCKETS];

void DeferredTrash::Clear()
{
	CurrentBucket = (CurrentBucket + 1) % NUM_BUCKETS;
	TrashBucket& bucket = Buckets[CurrentBucket];

	for (DescriptorAllocationEntry& entry : bucket.DescriptorAllocations) entry.DescriptorHeap->Release(entry.DescriptorAlloc);
	for (DescriptorEntry& entry : bucket.Descriptors) entry.DescriptorHeap->Release(entry.Descriptor);
	for (Shader* shader : bucket.Shaders) delete shader;
	for (Resource* resource : bucket.Resources) delete resource;

	bucket.DescriptorAllocations.clear();
	bucket.DXResources.clear();
	bucket.Descriptors.clear();
	bucket.Shaders.clear();
	bucket.Resources.clear();
}

void DeferredTrash::ClearAll()
{
	for (uint32_t i = 0; i < NUM_BUCKETS; i++) Clear();
}