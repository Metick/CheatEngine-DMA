#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"

namespace Hooks
{
	DWORD count_processes = 0;
	DWORD current_process = 0;
	PVMMDLL_PROCESS_INFORMATION info = NULL;

	//IsWow64Process
	typedef BOOL (WINAPI*IsWow64Process_t)(HANDLE, PBOOL);

	BOOL WINAPI hk_IsWow64Process(HANDLE hProcess, PBOOL Wow64Process)
	{
		//The out parameter has to be written. Leaving it alone made CE read an
		//uninitialised local, and if that came back non zero CE treated the target
		//as a 32 bit WOW64 process and clamped every scan to the low 4gb.
		if (Wow64Process)
			*Wow64Process = FALSE;
		return TRUE;
	}

	HANDLE hk_create_tool_help_32_snapshot(DWORD dwFlags, DWORD th32ProcessID)
	{
		return (HANDLE)0x66;
	}

	BOOL hk_process_32_first(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
	{
		if (info)
		{
			VMMDLL_MemFree(info);
			info = NULL;
		}

		count_processes = 0;
		current_process = 0;

		if (!VMMDLL_ProcessGetInformationAll(mem.vHandle, &info, &count_processes))
			return FALSE;

		return hk_process_32_next(hSnapshot, lppe);
	}

	BOOL hk_process_32_next(HANDLE hSnapshot, LPPROCESSENTRY32 lppe)
	{
		if (!info || current_process >= count_processes)
		{
			current_process = 0;
			return FALSE;
		}

		lppe->dwSize = sizeof(PROCESSENTRY32);
		lppe->th32ParentProcessID = info[current_process].dwPPID;
		lppe->th32ProcessID = info[current_process].dwPID;
		strncpy_s(lppe->szExeFile, sizeof(lppe->szExeFile), info[current_process].szNameLong, _TRUNCATE);
		current_process++;
		return TRUE;
	}
}
