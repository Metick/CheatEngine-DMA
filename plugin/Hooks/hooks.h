#pragma once
#include <cstdint>

#include "CheatEngine/cepluginsdk.h"
#include <TlHelp32.h>

namespace Hooks
{
	enum KPROCESSOR_MODE
	{
		KernelMode,
		UserMode,
	};

	inline void* detour_function(void* pSource, void* pDestination)
	{
		printf("Detouring function at %p to %p\n", pSource, pDestination);
		BYTE jmp_bytes[14] = {
			0xff, 0x25, 0x00, 0x00, 0x00, 0x00, // jmp [RIP+0x00000000]
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // RIP value
		};
		*reinterpret_cast<uint64_t*>(jmp_bytes + 6) = reinterpret_cast<uint64_t>(pDestination);

		void* stub = VirtualAlloc(0, sizeof(jmp_bytes), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (!stub)
		{
			printf("VirtualAlloc failed!\n");
			return nullptr;
		}

		memcpy(stub, pSource, sizeof(jmp_bytes));

		DWORD old_protect;
		if (VirtualProtect(pSource, sizeof(jmp_bytes), PAGE_EXECUTE_READWRITE, &old_protect))
		{
			memcpy(pSource, jmp_bytes, sizeof(jmp_bytes));
			VirtualProtect(pSource, sizeof(jmp_bytes), old_protect, &old_protect);
		}
		else
		{
			printf("VirtualProtect failed!\n");
			VirtualFree(stub, 0, MEM_RELEASE);
			return nullptr;
		}

		return stub;
	}

	//Mem.cpp
	extern void invalidate_memory_caches();
	extern SIZE_T hk_virtual_query(HANDLE hProcess, LPCVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwLength);
	extern BOOL hk_write(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
	extern BOOL hk_read(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T* lpNumberOfBytesRead);
	extern HANDLE hk_open_process(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId);

	//Process.cpp
	extern HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID);
	extern BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
	extern BOOL hk_process_32_next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe);
	extern BOOL WINAPI hk_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process);

	//Modules.cpp
	extern BOOL hk_module_32_next(HANDLE hSnapshot, LPMODULEENTRY32 lpme);
	extern BOOL hk_module_32_first(HANDLE hSnapshot, LPMODULEENTRY32 lpme);

	//Threads.cpp
	extern BOOL hk_thread_32_next(HANDLE hSnapshot, LPTHREADENTRY32 lpte);
	extern BOOL hk_thread_32_first(HANDLE hSnapshot, LPTHREADENTRY32 lpte);

	//Something still relies on OpenThread making this not working properly... gotto figure out what it is and implement it.
	extern HANDLE hk_open_thread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
	extern BOOL hk_get_thread_context(HANDLE hThread, PCONTEXT pContext);
	extern DWORD hk_resume_thread(HANDLE hThread);
	extern DWORD hk_suspend_thread(HANDLE hThread);
	extern BOOL hk_set_thread_context(HANDLE hThread, PCONTEXT pContext);
}
