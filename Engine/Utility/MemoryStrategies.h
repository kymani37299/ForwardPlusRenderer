#pragma once

#include <stack>
#include <list>
#include <mutex>

static constexpr size_t INVALID_ALLOCATION = static_cast<size_t>(-1);

struct PageAllocation
{
	size_t PageIndex = INVALID_ALLOCATION;
	size_t AllocationIndex = INVALID_ALLOCATION;
};

struct RangeAllocation
{
	size_t Start = INVALID_ALLOCATION;
	size_t NumElements = INVALID_ALLOCATION;
};

class ElementStrategy
{
public:
	ElementStrategy(size_t numElements) :
		m_NumElements(numElements) {}

	size_t Allocate()
	{
		size_t allocationIndex = INVALID_ALLOCATION;

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

	bool CanAllocate(size_t numElements)
	{
		bool canAlloocate = m_FreeAllocations.size() + (m_NumElements - m_NextAllocation) > numElements;
		return canAlloocate;
	}

	void Release(size_t alloc)
	{
		m_FreeAllocations.push(alloc);
	}

	void Clear()
	{
		while (!m_FreeAllocations.empty()) m_FreeAllocations.pop();
		m_NextAllocation = 0;
	}

private:
	size_t m_NumElements = 0;

	size_t m_NextAllocation = 0;
	std::stack<size_t> m_FreeAllocations;
};

class PageStrategy
{
public:
	PageStrategy(size_t numPages, size_t numElementsPerPage) :
		m_PageStrategy(numPages),
		m_NumElementsPerPage(numElementsPerPage) {}

	PageAllocation AllocatePage()
	{
		PageAllocation page;
		page.PageIndex = m_PageStrategy.Allocate();
		page.AllocationIndex = 0;
		return page;
	}

	bool CanAllocate(size_t numPages)
	{
		return m_PageStrategy.CanAllocate(numPages);
	}

	bool CanAllocateElement(const PageAllocation& page)
	{
		return page.AllocationIndex < m_NumElementsPerPage;
	}

	void ReleasePage(PageAllocation& page)
	{
		m_PageStrategy.Release(page.PageIndex);
		page.PageIndex = INVALID_ALLOCATION;
		page.AllocationIndex = INVALID_ALLOCATION;
	}

	size_t AllocateElement(PageAllocation& page)
	{
		size_t elementIndex = page.PageIndex * m_NumElementsPerPage + page.AllocationIndex;
		page.AllocationIndex++;
		return elementIndex;
	}

	void Clear() { m_PageStrategy.Clear(); }

private:
	ElementStrategy m_PageStrategy;
	size_t m_NumElementsPerPage = 0;
};

class RangeStrategy
{
public:
	RangeStrategy(size_t numElements):
		m_NumElements(numElements) {}

	RangeAllocation Allocate(size_t numElements)
	{
		if (numElements == 0) return { 0,0 };
		
		RangeAllocation alloc{};
		for (auto it = m_FreeRanges.begin(); it != m_FreeRanges.end(); it++)
		{
			RangeAllocation& candidate = *it;
			if (candidate.NumElements == numElements)
			{
				alloc = candidate;
				m_FreeRanges.erase(it);
				break;
			}
			else if (candidate.NumElements > numElements)
			{
				alloc.Start = candidate.Start;
				alloc.NumElements = numElements;

				candidate.Start += numElements;
				candidate.NumElements -= numElements;
				break;
			}
		}

		if (alloc.NumElements == INVALID_ALLOCATION && m_NextAllocation + numElements < m_NumElements)
		{
			alloc.Start = m_NextAllocation;
			alloc.NumElements = numElements;
			m_NextAllocation += numElements;
		}

		return alloc;
	}

	void Release(RangeAllocation& alloc)
	{
		if (alloc.NumElements != 0 && alloc.NumElements != INVALID_ALLOCATION)
			m_FreeRanges.push_back(alloc);
		
		alloc.Start = INVALID_ALLOCATION;
		alloc.NumElements = INVALID_ALLOCATION;
	}

	void Clear()
	{
		m_FreeRanges.clear();
		m_NextAllocation = 0;
	}
private:
	size_t m_NumElements = 0;
	size_t m_NextAllocation = 0;
	std::list<RangeAllocation> m_FreeRanges = {};
};

class RingRangeStrategy
{
public:
	RingRangeStrategy(size_t numElements) :
		m_NumElements(numElements) {}

	RangeAllocation Allocate(size_t numElements)
	{
		if (m_NextAllocation + numElements > m_NumElements)
			m_NextAllocation = 0;

		RangeAllocation alloc{};
		alloc.Start = m_NextAllocation;
		alloc.NumElements = numElements;

		m_NextAllocation += numElements;

		return alloc;
	}

	void Release(RangeAllocation& alloc)
	{
		ASSERT(0, "Do not use Release on transient descriptors!");
	}

	void Clear()
	{
		m_NextAllocation = 0;
	}
private:
	size_t m_NumElements = 0;
	size_t m_NextAllocation = 0;
};