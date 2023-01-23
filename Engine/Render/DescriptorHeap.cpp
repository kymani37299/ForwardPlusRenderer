#include "DescriptorHeap.h"

#include "Render/Device.h"
#include "Render/Shader.h"
#include "Render/Resource.h"

static ID3D12DescriptorHeap* CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, bool shaderVisible, size_t numDescriptors, size_t& incrementSize)
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

void DescriptorAllocation::Init()
{
	m_FirstGPUHandle = m_Transient ? m_Owner->m_HeapStartGPUTransient : m_Owner->m_HeapStartGPU;
	m_FirstGPUHandle.ptr += m_Alloc.Start * m_Owner->m_ElementSize;

	m_FirstCPUHandle = m_Transient ? m_Owner->m_HeapStartCPUTransient : m_Owner->m_HeapStartCPU;
	m_FirstCPUHandle.ptr += m_Alloc.Start * m_Owner->m_ElementSize;
}

void DescriptorAllocation::Release()
{
	ASSERT(m_Alloc.Start != INVALID_ALLOCATION, "Cannot release invalid allocation!");
	if (m_Transient)
		m_Owner->m_AllocStrategyTransient.Release(m_Alloc);
	else
		m_Owner->m_AllocStrategy.Release(m_Alloc);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetGPUHandle(size_t rangeIndex) const
{
	ASSERT(rangeIndex < m_Alloc.NumElements, "DescriptorHeapGPU memory overflow!");
	D3D12_GPU_DESCRIPTOR_HANDLE descriptor = m_FirstGPUHandle;
	descriptor.ptr += rangeIndex * m_Owner->m_ElementSize;
	return descriptor;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetCPUHandle(size_t rangeIndex) const
{
	ASSERT(rangeIndex < m_Alloc.NumElements, "DescriptorHeapGPU memory overflow!");
	D3D12_CPU_DESCRIPTOR_HANDLE descriptor = m_FirstCPUHandle;
	descriptor.ptr += rangeIndex * m_Owner->m_ElementSize;
	return descriptor;
}

DescriptorHeap::DescriptorHeap(bool gpuVisible, D3D12_DESCRIPTOR_HEAP_TYPE type, size_t numDescriptors, size_t numTransientDescriptors):
	m_GpuVisible(gpuVisible),
	m_AllocStrategy(numDescriptors),
	m_AllocStrategyTransient(numTransientDescriptors)
{
	m_Heap = CreateDescriptorHeap(type, gpuVisible, numDescriptors + numTransientDescriptors, m_ElementSize);

	m_HeapStartCPU = m_Heap->GetCPUDescriptorHandleForHeapStart();
	m_HeapStartCPUTransient.ptr = m_HeapStartCPU.ptr + numDescriptors * m_ElementSize;

	if (gpuVisible)
	{
		m_HeapStartGPU = m_Heap->GetGPUDescriptorHandleForHeapStart();
		m_HeapStartGPUTransient.ptr = m_HeapStartGPU.ptr + numDescriptors * m_ElementSize;
	}
}

DescriptorAllocation DescriptorHeap::Allocate(size_t numDescriptors)
{
	const RangeAllocation heapAlloc = m_AllocStrategy.Allocate(numDescriptors);
	ASSERT(heapAlloc.Start != INVALID_ALLOCATION, "DescriptorHeapGPU memory overflow!");
	return DescriptorAllocation{ false, heapAlloc , this };
}

DescriptorAllocation DescriptorHeap::AllocateTransient(size_t numDescriptors)
{
	const RangeAllocation heapAlloc = m_AllocStrategyTransient.Allocate(numDescriptors);
	ASSERT(heapAlloc.Start != INVALID_ALLOCATION, "DescriptorHeapGPU transient memory overflow!");
	return DescriptorAllocation{true, heapAlloc , this};
}