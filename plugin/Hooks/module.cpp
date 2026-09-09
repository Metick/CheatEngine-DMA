#include "hooks.h"
#include "DMALibrary/Memory/Memory.h"

namespace Hooks
{
	PVMMDLL_MAP_MODULE module_info = NULL;
	DWORD current_module = 0;

	static const char* safe_str(LPSTR s)
	{
		return s ? s : "";
	}

	BOOL hk_module_32_first(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
	{
		if (module_info)
		{
			VMMDLL_MemFree(module_info);
			module_info = NULL;
		}

		current_module = 0;

		if (!VMMDLL_Map_GetModuleU(mem.vHandle, mem.current_process.PID, &module_info, VMMDLL_MODULE_FLAG_NORMAL))
			return FALSE;

		//Hand off to _next so the first module is emitted exactly once. The old
		//code filled entry 0 here without advancing, so _next returned it again.
		return hk_module_32_next(hSnapshot, lpme);
	}

	BOOL hk_module_32_next(HANDLE hSnapshot, LPMODULEENTRY32 lpme)
	{
		if (!module_info || current_module >= module_info->cMap)
		{
			current_module = 0;
			return FALSE;
		}

		const VMMDLL_MAP_MODULEENTRY& entry = module_info->pMap[current_module];

		lpme->dwSize = sizeof(MODULEENTRY32);
		lpme->th32ProcessID = mem.current_process.PID;
		lpme->hModule = (HMODULE)entry.vaBase;
		lpme->modBaseSize = entry.cbImageSize;
		lpme->modBaseAddr = (BYTE*)entry.vaBase;
		//szModule is 256 bytes and szExePath is MAX_PATH, a module path can be
		//longer than either, so these have to be bounded copies.
		strncpy_s(lpme->szModule, sizeof(lpme->szModule), safe_str(entry.uszText), _TRUNCATE);
		strncpy_s(lpme->szExePath, sizeof(lpme->szExePath), safe_str(entry.uszFullName), _TRUNCATE);
		current_module++;
		return TRUE;
	}
}
