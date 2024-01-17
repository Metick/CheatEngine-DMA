// example-c.cpp : Defines the entry point for the DLL application.
//

//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
// Windows Header Files:

#include <windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include "CheatEngine/cepluginsdk.h"

#include <DMALibrary/Memory/Memory.h>
#include "Memory/vad.h"
#include "Memory/memmy.h"

int PointerReassignmentPluginID = -1;
int MainMenuPluginID = -1;

ExportedFunctions Exported;

void __stdcall mainmenuplugin(void)
{
	Exported.ShowMessage("Main menu plugin");
	return;
}

BOOL APIENTRY DllMain(HANDLE hModule,
                      DWORD ul_reason_for_call,
                      LPVOID lpReserved
)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}

	return TRUE;
}

BOOL __stdcall CEPlugin_GetVersion(PPluginVersion pv, int sizeofpluginversion)
{
	pv->version = CESDK_VERSION;
	pv->pluginname = "Methicc's DMA plugin";
	return TRUE;
}

c_memory_map<vad_info> memoryMap;
std::vector<vad_info> vad_infos;

HANDLE hk_open_process(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId)
{
	if (mem.Init(dwProcessId))
	{
		PVMMDLL_MAP_VAD vads;
		memoryMap.clear();
		vad_infos.clear();

		if (!VMMDLL_Map_GetVadW(mem.vHandle, mem.current_process.PID, true, &vads))
			return false;
		std::vector<vad_info> vad_infos;

		for (size_t i = 0; i < vads->cMap; i++)
		{
			auto vad = vads->pMap[i];
			vad_infos.push_back(vad_info(vad.wszText, vad.vaStart, vad.vaEnd, vad));
		}

		for (size_t i = 0; i < vad_infos.size(); i++)
		{
			size_t regionSize = vad_infos[i].get_end() - vad_infos[i].get_start() + 1;
			//smaller than max int
			memoryMap.add(c_memory_region<vad_info>(vad_infos[i], vad_infos[i].get_start(), regionSize));
		}

		//Get List of Processes

		return (HANDLE)0x69;
	}

	return false;
}

BOOL hk_read(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead)
{
	return mem.Read((UINT64)lpBaseAddress, lpBuffer, nSize, (PDWORD)lpNumberOfBytesRead);
}

bool hk_write(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead)
{
	return mem.Write((UINT64)lpBaseAddress, lpBuffer, nSize);
}

SIZE_T hk_virtual_query(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength)
{
	MEMORY_BASIC_INFORMATION meminfo;
	//Memory in VirtualQuery Is always rounded down, getMemoryRegionContaining will find the nearest (rounded down) region that contains the address.
	//or if it's equal return the exact region.
	c_memory_region<vad_info> region = memoryMap.get_memory_region_containing((uintptr_t)lpAddress);
	if (region.get_region_size() > 0)
	{
		//We have Protection hardcoded rn, because we're DMA and we can read all pages. uncomment the line in get_protection & comment the PAGE_READWRITE to use the real protection that the page has.
		auto found_vad = region.get_object();
		auto rangeStart = region.get_region_start();
		auto rangeEnd = region.get_region_end();
		auto size = rangeEnd - rangeStart + 1;
		meminfo.BaseAddress = reinterpret_cast<PVOID>(rangeStart);
		meminfo.AllocationBase = reinterpret_cast<PVOID>(rangeStart);
		meminfo.AllocationProtect = found_vad.get_protection();
		meminfo.RegionSize = size;
		meminfo.State = found_vad.get_state(); // Assuming State field in vad_info corresponds to State
		meminfo.Protect = found_vad.get_protection(); // Assuming Protect field in vad_info corresponds to Protect
		meminfo.Type = found_vad.get_type(); // Assuming Type field in vad_info corresponds to Type
		meminfo.PartitionId = 0;

		memcpy(lpBuffer, &meminfo, sizeof(meminfo));
		return sizeof(meminfo);
	}

	return 0;
}

void __stdcall PointersReassigned(int reserved)
{
	printf("Pointers got modified");
	auto open_process = Exported.OpenProcess;
	auto read_process_memory = Exported.ReadProcessMemory;
	auto write_process_memory = Exported.WriteProcessMemory;
	auto virtual_query = Exported.VirtualQueryEx;

	printf("Hooking Open Process 0x%p\n", open_process);
	*(uintptr_t*)(open_process) = (uintptr_t)&hk_open_process;

	printf("Hooking Read 0x%p\n", read_process_memory);
	*(uintptr_t*)(read_process_memory) = (uintptr_t)&hk_read;

	printf("Hooking Write 0x%p\n", write_process_memory);
	*(uintptr_t*)(write_process_memory) = (uintptr_t)&hk_write;

	printf("Hooking Virtual Query 0x%p\n", virtual_query);
	*(uintptr_t*)(virtual_query) = (uintptr_t)&hk_virtual_query;
}

HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID)
{
	return (HANDLE)CreateToolhelp32Snapshot(dwFlags, th32ProcessID);
}

DWORD count_processes = 0;
DWORD current_process = 0;
PVMMDLL_PROCESS_INFORMATION info = NULL;

BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
	info = NULL;
	count_processes = 0;
	if (!VMMDLL_ProcessGetInformationAll(mem.vHandle, &info, &count_processes))
		return false;
	lppe->dwSize = sizeof(PROCESSENTRY32);
	lppe->th32ParentProcessID = info[current_process].dwPPID;
	lppe->th32ProcessID = info[current_process].dwPID;
	strcpy(lppe->szExeFile, info[current_process].szNameLong);
	current_process++;
	return true;
}

BOOL hk_process_32_next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
{
	if (current_process >= count_processes)
	{
		current_process = 0;
		return false;
	}

	lppe->dwSize = sizeof(PROCESSENTRY32);
	lppe->th32ParentProcessID = info[current_process].dwPPID;
	lppe->th32ProcessID = info[current_process].dwPID;
	strcpy(lppe->szExeFile, info[current_process].szNameLong);
	current_process++;
	return true;
}

PVMMDLL_MAP_MODULE module_info = NULL;
DWORD current_module = 0;

BOOL hk_module_32_first(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
	module_info = NULL;
	current_module = 0;
	if (!VMMDLL_Map_GetModuleU(mem.vHandle, mem.current_process.PID, &module_info, VMMDLL_MODULE_FLAG_NORMAL))
		return false;

	lpme->dwSize = sizeof(MODULEENTRY32);
	lpme->th32ProcessID = mem.current_process.PID;
	lpme->hModule = (HMODULE)module_info->pMap[current_module].vaBase;
	lpme->modBaseSize = module_info->pMap[current_module].cbImageSize;
	lpme->modBaseAddr = (BYTE*)module_info->pMap[current_module].vaBase;
	strcpy(lpme->szModule, module_info->pMap[current_module].uszText);
	return true;
}

BOOL hk_module_32_next(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
{
	if (current_module >= module_info->cMap)
	{
		current_module = 0;
		return false;
	}

	lpme->dwSize = sizeof(MODULEENTRY32);
	lpme->th32ProcessID = mem.current_process.PID;
	lpme->hModule = (HMODULE)module_info->pMap[current_module].vaBase;
	lpme->modBaseSize = module_info->pMap[current_module].cbImageSize;
	lpme->modBaseAddr = (BYTE*)module_info->pMap[current_module].vaBase;
	strcpy(lpme->szModule, module_info->pMap[current_module].uszText);
	current_module++;
	return true;
}

BOOL __stdcall CEPlugin_InitializePlugin(PExportedFunctions ef, int pluginid)
{
	MAINMENUPLUGIN_INIT init1;
	POINTERREASSIGNMENTPLUGIN_INIT init4;

	//open console
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("initializing\n");
	auto open_process = ef->OpenProcess;
	auto read_process_memory = ef->ReadProcessMemory;
	auto write_process_memory = ef->WriteProcessMemory;
	auto virtual_query = ef->VirtualQueryEx;
	auto create_tool_help32 = ef->CreateToolhelp32Snapshot;
	auto process_32_first = ef->Process32First;
	auto process_32_next = ef->Process32Next;
	auto module_32_first = ef->Module32First;
	auto module_32_next = ef->Module32Next;

	printf("Hooking Open Process 0x%p\n", open_process);
	*(uintptr_t*)(open_process) = (uintptr_t)&hk_open_process;

	printf("Hooking Read 0x%p\n", read_process_memory);
	*(uintptr_t*)(read_process_memory) = (uintptr_t)&hk_read;

	printf("Hooking Write 0x%p\n", write_process_memory);
	*(uintptr_t*)(write_process_memory) = (uintptr_t)&hk_write;

	printf("Hooking Virtual Query 0x%p\n", virtual_query);
	*(uintptr_t*)(virtual_query) = (uintptr_t)&hk_virtual_query;

	printf("Hooking CreateToolhelp32Snapshot 0x%p\n", create_tool_help32);
	*(uintptr_t*)(create_tool_help32) = (uintptr_t)&hk_create_tool_help_32_snapshot;

	printf("Hooking Process32First 0x%p\n", process_32_first);
	*(uintptr_t*)(process_32_first) = (uintptr_t)&hk_process_32_first;

	printf("Hooking Process32Next 0x%p\n", process_32_next);
	*(uintptr_t*)(process_32_next) = (uintptr_t)&hk_process_32_next;

	printf("Hooking Module32First 0x%p\n", module_32_first);
	*(uintptr_t*)(module_32_first) = (uintptr_t)&hk_module_32_first;

	printf("Hooking Module32Next 0x%p\n", module_32_next);
	*(uintptr_t*)(module_32_next) = (uintptr_t)&hk_module_32_next;

	printf("Initialize DMA in advance\n");
	mem.Init("", true);

	//TODO: fix this, this doesn't seem to work for me, i don't know why.
	/*init4.callbackroutine = PointersReassigned;
	PointerReassignmentPluginID = Exported.RegisterFunction(pluginid, ptFunctionPointerchange, &init4); //adds a plugin menu item to the memory view
	if (PointerReassignmentPluginID == -1)
	{
		Exported.ShowMessage("Failure to register the pointer reassignment plugin");
		return FALSE;
	}*/

	init1.name = "DMA Methicc CE Plugin";
	init1.callbackroutine = mainmenuplugin;
	ef->RegisterFunction(pluginid, ptMainMenu, &init1);
	printf("Initialized Methicc's CE DMA plugin\n");
	Exported = *ef;
	return TRUE;
}

BOOL __stdcall CEPlugin_DisablePlugin(void)
{
	exit(0);
	return TRUE;
}
