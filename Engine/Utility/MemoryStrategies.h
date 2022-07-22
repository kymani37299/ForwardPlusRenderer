#pragma once

#include <stack>

template<typename AllocType>
class ElementStrategy
{
public:
	static constexpr AllocType INVALID = static_cast<AllocType>(-1);

	ElementStrategy(AllocType numElements) :
		m_NumElements(numElements) {}

	AllocType Allocate()
	{
		AllocType allocationIndex = INVALID;

		if (!m_FreeAllocations.empty())
		{
			allocationIndex = m_FreeAllocations.top();
			m_FreeAllocations.pop();
		}
		else if (m_NextAllocation < m_NumElements)
		{
			allocationIndex = m_NextAllocation;
			m_NextAllocation++;
		}
		return allocationIndex;
	}

	bool CanAllocate(AllocType numElements)
	{
		return m_FreeAllocations.size() + (m_NumElements - m_NextAllocation) > numElements;
	}

	void Release(AllocType alloc)
	{
		m_FreeAllocations.push(alloc);
	}

	void Clear()
	{
		while (!m_FreeAllocations.empty()) m_FreeAllocations.pop();
		m_NextAllocation = 0;
	}

private:
	AllocType m_NumElements = 0;

	AllocType m_NextAllocation = 0;
	std::stack<AllocType> m_FreeAllocations;
};

template<typename AllocType>
class PageStrategy
{
public:
	struct Page
	{
		AllocType PageIndex;
		AllocType AllocationIndex;
	};

	PageStrategy(AllocType numPages, AllocType numElementsPerPage) :
		m_PageStrategy(numPages),
		m_NumElementsPerPage(numElementsPerPage) {}

	Page AllocatePage()
	{
		Page page;
		page.PageIndex = m_PageStrategy.Allocate();
		page.AllocationIndex = 0;
		return page;
	}

	bool CanAllocate(AllocType numPages)
	{
		return m_PageStrategy.CanAllocate(numPages);
	}

	void ReleasePage(Page& page)
	{
		m_PageStrategy.Release(page.PageIndex);
		page.PageIndex = ElementStrategy<AllocType>::INVALID;
		page.AllocationIndex = ElementStrategy<AllocType>::INVALID;
	}

	AllocType AllocateElement(Page& page)
	{
		AllocType elementIndex = page.PageIndex * m_NumElementsPerPage + page.AllocationIndex;
		page.AllocationIndex++;
		return elementIndex;
	}

	void Clear() { m_PageStrategy.Clear(); }

private:
	ElementStrategy<AllocType> m_PageStrategy;
	AllocType m_NumElementsPerPage = 0;
};