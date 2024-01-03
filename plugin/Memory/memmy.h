#pragma once
#include <iostream>
#include <assert.h>

template <typename T>
class MemoryRegion
{
public:
	MemoryRegion() : regionStart(0), size(0)
	{
	}

	MemoryRegion(T userObject, uintptr_t regionStart, size_t size)
		: userObject(userObject), regionStart(regionStart), size(size)
	{
	}

	T getUserObject() const
	{
		return userObject;
	}

	uintptr_t getRegionStart() const
	{
		return regionStart;
	}

	uintptr_t getRegionEnd() const
	{
		return regionStart + size - 1;
	}

	size_t getRegionSize() const
	{
		return size;
	}

	bool operator<(const MemoryRegion& other) const
	{
		return this->regionStart < other.regionStart;
	}

	bool operator==(const MemoryRegion& other) const
	{
		return this->regionStart == other.regionStart;
	}

	bool contains(uintptr_t address) const
	{
		return address >= getRegionStart() && address < getRegionEnd() + 1;
	}

private:
	T userObject;
	uintptr_t regionStart;
	size_t size;
};

// rewritten from Java to C++
// https://github.com/idkfrancis/ceserver-pcileech/blob/main/src/main/java/iflores/ceserver/pcileech/MemoryMap.java#L50
template <typename T>
class MemoryMap
{
public:
	MemoryMap() = default;

	void add(const MemoryRegion<T>& newEntry)
	{
		regions.push_back(newEntry);
	}

	void clear()
	{
		regions.clear();
	}

	MemoryRegion<T> getMemoryRegionContaining(uintptr_t address) const
	{
		T value;
		MemoryRegion<T> floorEntry = floor(address);
		if (floorEntry.getRegionSize() > 0)
		{
			if (floorEntry.contains(address))
			{
				return floorEntry;
			}
			MemoryRegion<T> ceilEntry = ceil(address);
			if (ceilEntry.getRegionSize() > 0)
			{
				// Adjusted to handle large regions
				return MemoryRegion<T>(value, floorEntry.getRegionEnd() + 1, ceilEntry.getRegionStart() - floorEntry.getRegionEnd() - 1);
			}
			// Past end
			return MemoryRegion<T>(value, 0, 0);
		}
		MemoryRegion<T> ceilEntry = ceil(address);
		if (ceilEntry.getRegionSize() > 0)
		{
			return MemoryRegion<T>(value, 0, ceilEntry.getRegionStart());
		}
		return MemoryRegion<T>(value, 0, 0);
	}

private:
	MemoryRegion<T> floor(uintptr_t address) const
	{
		T value;
		auto it = std::lower_bound(regions.begin(), regions.end(), address,
		                           [](const MemoryRegion<T>& region, uintptr_t addr)
		                           {
			                           return region.getRegionEnd() < addr;
		                           });
		if (it != regions.end() && it->contains(address))
		{
			return *it;
		}
		return it == regions.begin() ? MemoryRegion<T>(value, 0, 0) : *(--it);
	}

	MemoryRegion<T> ceil(uintptr_t address) const
	{
		T value;
		auto it = std::upper_bound(regions.begin(), regions.end(), address,
		                           [](uintptr_t addr, const MemoryRegion<T>& region)
		                           {
			                           return addr < region.getRegionStart();
		                           });
		return it == regions.end() ? MemoryRegion<T>(value, 0, 0) : *it;
	}

	bool regionOverlaps(const MemoryRegion<T>& a, const MemoryRegion<T>& b) const
	{
		if (a.getRegionStart() > b.getRegionEnd() || b.getRegionStart() > a.getRegionEnd())
		{
			return false;
		}
		return true;
	}

	std::vector<MemoryRegion<T>> regions;
};
