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
	AllocStrategy(numDescriptors)
{
	Heap = CreateDescriptorHeap(type, false, numDescriptors, ElementSize);
	HeapStart = Heap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapCPU::Alloc()
{
	size_t index = AllocStrategy.Allocate();
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor = HeapStart;
	descriptor.ptr += index * ElementSize;
	return descriptor;
}

void DescriptorHeapCPU::Release(D3D12_CPU_DESCRIPTOR_HANDLE& handle)
{
	size_t index = handle.ptr;
	index -= HeapStart.ptr;
	index /= ElementSize;
	AllocStrategy.Release(index);

	handle.ptr = CPUAllocStrategy::INVALID;
}

DescriptorHeapGPU::DescriptorHeapGPU(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numPages, size_t numDescriptors) :
	AllocStrategy(numPages, numDescriptors)
{
	Heap = CreateDescriptorHeap(type, true, numPages * numDescriptors, ElementSize);
	PageSize = ElementSize * numDescriptors;
	HeapStartCPU = Heap->GetCPUDescriptorHandleForHeapStart();
	HeapStartGPU = Heap->GetGPUDescriptorHandleForHeapStart();
}

GPUAllocStrategy::Page DescriptorHeapGPU::NewPage() { return AllocStrategy.AllocatePage(); }
void DescriptorHeapGPU::ReleasePage(GPUAllocStrategy::Page& page) { AllocStrategy.ReleasePage(page); }

DescriptorHeapGPU::Allocation DescriptorHeapGPU::Alloc(GPUAllocStrategy::Page& page)
{
	size_t elementIndex = AllocStrategy.AllocateElement(page);

	Allocation alloc{};
	alloc.CPUHandle = HeapStartCPU;
	alloc.GPUHandle = HeapStartGPU;
	alloc.CPUHandle.ptr += ElementSize * elementIndex;
	alloc.GPUHandle.ptr += ElementSize * elementIndex;

	return alloc;
}

uint32_t DeferredTrash::CurrentBucket = 0;
DeferredTrash::TrashBucket DeferredTrash::Buckets[DeferredTrash::NUM_BUCKETS];

void DeferredTrash::Clear()
{
	CurrentBucket = (CurrentBucket + 1) % NUM_BUCKETS;
	TrashBucket& bucket = Buckets[CurrentBucket];

	for (DescriptorPagesEntry& entry : bucket.DescriptorPages) entry.DescriptorHeap->ReleasePage(entry.Page);
	for (DescriptorEntry& entry : bucket.Descriptors) entry.DescriptorHeap->Release(entry.Descriptor);
	for (Shader* shader : bucket.Shaders) delete shader;
	for (Resource* resource : bucket.Resources) delete resource;

	bucket.DescriptorPages.clear();
	bucket.DXResources.clear();
	bucket.Descriptors.clear();
	bucket.Shaders.clear();
	bucket.Resources.clear();
}

void DeferredTrash::ClearAll()
{
	for (uint32_t i = 0; i < NUM_BUCKETS; i++) Clear();
}