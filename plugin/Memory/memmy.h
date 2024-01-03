#pragma once
#include <iostream>
#include <assert.h>

template <typename T>
class c_memory_region
{
public:
	c_memory_region() : region_start(0), size(0)
	{
	}

	c_memory_region(T user_object, uintptr_t region_start, size_t size)
		: user_object(user_object), region_start(region_start), size(size)
	{
	}

	T get_object() const
	{
		return user_object;
	}

	uintptr_t get_region_start() const
	{
		return region_start;
	}

	uintptr_t get_region_end() const
	{
		return region_start + size - 1;
	}

	size_t get_region_size() const
	{
		return size;
	}

	bool operator<(const c_memory_region& other) const
	{
		return this->region_start < other.region_start;
	}

	bool operator==(const c_memory_region& other) const
	{
		return this->region_start == other.region_start;
	}

	bool contains(uintptr_t address) const
	{
		return address >= get_region_start() && address < get_region_end() + 1;
	}

private:
	T user_object;
	uintptr_t region_start;
	size_t size;
};

// rewritten from Java to C++
// https://github.com/idkfrancis/ceserver-pcileech/blob/main/src/main/java/iflores/ceserver/pcileech/MemoryMap.java#L50
template <typename T>
class c_memory_map
{
public:
	c_memory_map() = default;

	void add(const c_memory_region<T>& newEntry)
	{
		regions.push_back(newEntry);
	}

	void clear()
	{
		regions.clear();
	}

	c_memory_region<T> get_memory_region_containing(uintptr_t address) const
	{
		T value;
		c_memory_region<T> floor_entry = floor(address);
		if (floor_entry.get_region_size() > 0)
		{
			if (floor_entry.contains(address))
			{
				return floor_entry;
			}
			c_memory_region<T> ceil_entry = ceil(address);
			if (ceil_entry.get_region_size() > 0)
			{
				// Adjusted to handle large regions
				return c_memory_region<T>(value, floor_entry.get_region_end() + 1, ceil_entry.get_region_start() - floor_entry.get_region_end() - 1);
			}
			// Past end
			return c_memory_region<T>(value, 0, 0);
		}
		c_memory_region<T> ceil_entry = ceil(address);
		if (ceil_entry.get_region_size() > 0)
		{
			return c_memory_region<T>(value, 0, ceil_entry.get_region_start());
		}
		return c_memory_region<T>(value, 0, 0);
	}

private:
	c_memory_region<T> floor(uintptr_t address) const
	{
		T value;
		auto it = std::lower_bound(regions.begin(), regions.end(), address,
		                           [](const c_memory_region<T>& region, uintptr_t addr)
		                           {
			                           return region.get_region_end() < addr;
		                           });
		if (it != regions.end() && it->contains(address))
		{
			return *it;
		}
		return it == regions.begin() ? c_memory_region<T>(value, 0, 0) : *(--it);
	}

	c_memory_region<T> ceil(uintptr_t address) const
	{
		T value;
		auto it = std::upper_bound(regions.begin(), regions.end(), address,
		                           [](uintptr_t addr, const c_memory_region<T>& region)
		                           {
			                           return addr < region.get_region_start();
		                           });
		return it == regions.end() ? c_memory_region<T>(value, 0, 0) : *it;
	}

	bool region_overlaps(const c_memory_region<T>& a, const c_memory_region<T>& b) const
	{
		if (a.get_region_start() > b.get_region_end() || b.get_region_start() > a.get_region_end())
		{
			return false;
		}
		return true;
	}

	std::vector<c_memory_region<T>> regions;
};
