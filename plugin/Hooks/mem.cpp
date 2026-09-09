#include <algorithm>
#include <list>
#include <map>
#include <mutex>

#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"
#include "Memory/memmy.h"
#include "Memory/vad.h"

namespace Hooks
{
	//Everything below 0x7ff000000000 is resolved from the VAD map, everything above
	//it from the PTE map.
	static const uintptr_t VAD_PTE_SPLIT = 0x7ff000000000;

	//How long a cached VAD map stays usable. CE issues thousands of VirtualQueryEx
	//calls per scan and re-pulling the map over DMA for each of them is far too
	//slow, but the map must not go stale enough to miss fresh allocations either.
	static const ULONGLONG REGION_CACHE_MS = 10000;

	static std::map<int, std::pair<ULONGLONG, std::list<c_memory_region<vad_info>>>> region_cache;
	static PVMMDLL_MAP_PTE pte_map = NULL;
	static int pte_map_pid = 0;

	//CE runs one scan worker per cpu, so every cache below is shared across threads.
	static std::mutex cache_mutex;

	void invalidate_memory_caches()
	{
		std::lock_guard<std::mutex> lock(cache_mutex);

		region_cache.clear();

		if (pte_map)
		{
			VMMDLL_MemFree(pte_map);
			pte_map = NULL;
		}
		pte_map_pid = 0;
	}

	HANDLE hk_open_process(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId)
	{
		//Attaching to a different process invalidates every cached map, otherwise
		//the next scan runs against the regions of the previous target.
		invalidate_memory_caches();

		if (mem.Init((int)dwProcessId))
			return (HANDLE)0x69;

		return NULL;
	}

	BOOL hk_read(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead)
	{
		if (lpNumberOfBytesRead)
			*lpNumberOfBytesRead = 0;

		if (!lpBuffer || !nSize || nSize > MAXDWORD)
			return FALSE;

		//VMMDLL leaves the bytes of pages it could not read untouched, so without
		//this CE would scan whatever the previous chunk left in the buffer.
		memset(lpBuffer, 0, nSize);

		DWORD read = 0;
		mem.Read((UINT64)lpBaseAddress, lpBuffer, nSize, &read);

		//The count belonging to the caller is a SIZE_T. Casting its address to
		//PDWORD and letting VMMDLL write it left the upper four bytes as garbage.
		if (lpNumberOfBytesRead)
			*lpNumberOfBytesRead = read;

		//Report success only on a complete read. The PDWORD overload of mem.Read
		//returns true for a partial or empty read as well.
		return read == nSize;
	}

	BOOL hk_write(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead)
	{
		if (lpNumberOfBytesRead)
			*lpNumberOfBytesRead = 0;

		if (!lpBuffer || !nSize)
			return FALSE;

		if (!mem.Write((UINT64)lpBaseAddress, lpBuffer, nSize))
			return FALSE;

		if (lpNumberOfBytesRead)
			*lpNumberOfBytesRead = nSize;

		return TRUE;
	}

	static std::list<c_memory_region<vad_info>> get_memory_region()
	{
		std::list<c_memory_region<vad_info>> result = { };
		PVMMDLL_MAP_VAD vads = nullptr;

		if (!VMMDLL_Map_GetVadW(mem.vHandle, mem.current_process.PID, true, &vads))
			return { };

		for (DWORD i = 0; i < vads->cMap; i++)
		{
			const VMMDLL_MAP_VADENTRY& vad = vads->pMap[i];
			vad_info entry(vad.wszText ? vad.wszText : L"", vad.vaStart, vad.vaEnd, vad);
			result.push_back(c_memory_region<vad_info>(entry, vad.vaStart, vad.vaEnd - vad.vaStart + 1));
		}

		VMMDLL_MemFree(vads);
		return result;
	}

	//Returns the region containing lpAddress, or the first region above it when the
	//address falls in a hole. Returns false once lpAddress is past the last region.
	static bool VirtualQueryImpl_(uintptr_t lpAddress, c_memory_region<vad_info>* ret)
	{
		std::lock_guard<std::mutex> lock(cache_mutex);

		auto entry = region_cache.find(mem.current_process.PID);
		if (entry == region_cache.end() || GetTickCount64() - entry->second.first > REGION_CACHE_MS)
		{
			auto regions = get_memory_region();
			if (regions.empty())
				return false;

			region_cache[mem.current_process.PID] = std::make_pair(GetTickCount64(), std::move(regions));
			entry = region_cache.find(mem.current_process.PID);
		}

		//Bind by reference. Copying the list on every call cloned every vad_info,
		//wstring included, thousands of times per scan.
		const std::list<c_memory_region<vad_info>>& regions = entry->second.second;

		//The old lower_bound predicate returned the first region starting *after*
		//lpAddress, which silently skipped every region beginning exactly where the
		//previous one ended.
		auto it = std::find_if(regions.begin(), regions.end(),
		                       [lpAddress](const c_memory_region<vad_info>& region)
		                       {
			                       return region.get_region_end() >= lpAddress;
		                       });
		if (it == regions.end())
			return false;

		*ret = *it;
		return true;
	}

	//Caller must hold cache_mutex.
	static PVMMDLL_MAP_PTE get_pte_map()
	{
		if (pte_map && pte_map_pid == mem.current_process.PID)
			return pte_map;

		if (pte_map)
		{
			VMMDLL_MemFree(pte_map);
			pte_map = NULL;
		}

		if (!VMMDLL_Map_GetPteU(mem.vHandle, mem.current_process.PID, TRUE, &pte_map))
		{
			printf("Failed to get PTE\n");
			pte_map = NULL;
			return NULL;
		}

		pte_map_pid = mem.current_process.PID;
		return pte_map;
	}

	SIZE_T hk_virtual_query(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength)
	{
		if (!lpBuffer || dwLength < sizeof(MEMORY_BASIC_INFORMATION))
			return 0;

		MEMORY_BASIC_INFORMATION info { };
		const uintptr_t address = reinterpret_cast<uintptr_t>(lpAddress);
		bool valid = false;

		if (address > VAD_PTE_SPLIT)
		{
			std::lock_guard<std::mutex> lock(cache_mutex);

			PVMMDLL_MAP_PTE map = get_pte_map();
			if (!map)
				return 0;

			for (DWORD i = 0; i < map->cMap; i++)
			{
				const VMMDLL_MAP_PTEENTRY& entry = map->pMap[i];
				const uintptr_t start = (uintptr_t)entry.vaBase;
				const size_t length = (size_t)entry.cPages << 12;
				const uintptr_t end = start + length; //exclusive

				if (address >= end)
					continue;

				if (address < start)
				{
					//Hole before this entry, report it free so CE walks straight on.
					info.BaseAddress = const_cast<PVOID>(lpAddress);
					info.AllocationBase = NULL;
					info.AllocationProtect = 0;
					info.RegionSize = start - address;
					info.Protect = PAGE_NOACCESS;
					info.State = MEM_FREE;
					info.Type = 0;
				}
				else
				{
					//The old code took the size from the base of the *next* entry,
					//which stretched the region across unmapped holes (so the read
					//that followed failed) and read one past the array on the last
					//entry.
					info.BaseAddress = (PVOID)start;
					info.AllocationBase = (PVOID)start;
					info.RegionSize = length;
					info.State = MEM_COMMIT;
					info.Protect = PAGE_EXECUTE_READWRITE;
					info.AllocationProtect = info.Protect;

					if (entry.uszText && entry.uszText[0])
					{
						if (strncmp(entry.uszText, "HEAP", 4) == 0 || strncmp(entry.uszText, "[HEAP", 5) == 0)
							info.Type = MEM_PRIVATE;
						else
							info.Type = MEM_IMAGE;
					}
					else
					{
						//Was MEM_MAPPED, which CE skips by default.
						info.Type = MEM_PRIVATE;
					}
				}

				info.PartitionId = 0;
				valid = true;
				break;
			}

			if (!valid)
				return 0; //past the last mapped page, ends the walk

			memcpy(lpBuffer, &info, sizeof(info));
			return sizeof(info);
		}

		c_memory_region<vad_info> vinfo;
		if (!VirtualQueryImpl_(address, &vinfo))
			return 0;

		vad_info found_vad = vinfo.get_object();
		const uintptr_t range_start = vinfo.get_region_start();

		if (!vinfo.contains(address))
		{
			//Hole below this region.
			info.BaseAddress = const_cast<PVOID>(lpAddress);
			info.AllocationBase = NULL;
			info.AllocationProtect = 0;
			info.RegionSize = range_start - address;
			info.State = MEM_FREE;
			info.Protect = PAGE_NOACCESS;
			info.Type = 0;
		}
		else
		{
			info.BaseAddress = reinterpret_cast<PVOID>(range_start);
			info.AllocationBase = reinterpret_cast<PVOID>(range_start);
			info.AllocationProtect = found_vad.get_protection();
			info.RegionSize = vinfo.get_region_size();
			info.State = found_vad.get_state();
			info.Protect = found_vad.get_protection();
			info.Type = found_vad.get_type();
		}

		info.PartitionId = 0;
		memcpy(lpBuffer, &info, sizeof(info));
		return sizeof(info);
	}
}
