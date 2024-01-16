#pragma once
#include <string>

class vad_info
{
private:
	std::wstring _name;
	uintptr_t _start;
	uintptr_t _end;
	uint32_t _type;
	uint32_t _protection;
	uint32_t _fImage;
	uint32_t _fFile;
	uint32_t _fPageFile;
	uint32_t _fPrivateMemory;
	uint32_t _fTeb;
	uint32_t _fStack;
	uint32_t _fSpare;
	uint32_t _HeapNum;
	uint32_t _fHeap;
	uint32_t _cwszDescription;
	uint32_t _commitCharge;
	uint32_t _memCommit;
	VMMDLL_MAP_VADENTRY _vad;

	int getValue(int mask, int start, int length)
	{
		return (mask << (32 - start - length)) >> (32 - length);
	}

public:
	vad_info() = default;

	vad_info(std::wstring name, uintptr_t start, uintptr_t end, VMMDLL_MAP_VADENTRY vad)
	{
		_name = name;
		_start = start;
		_end = end;
		_type = vad.VadType;
		_protection = vad.Protection;
		_fImage = vad.fImage;
		_fFile = vad.fFile;
		_fPageFile = vad.fPageFile;
		_fPrivateMemory = vad.fPrivateMemory;
		_fTeb = vad.fTeb;
		_fStack = vad.fStack;
		_fSpare = vad.fSpare;
		_HeapNum = vad.HeapNum;
		_fHeap = vad.fHeap;
		_cwszDescription = vad.cwszDescription;
		_commitCharge = vad.CommitCharge;
		_memCommit = vad.MemCommit;
		_vad = vad;
	}

	std::wstring get_name()
	{
		return _name;
	}

	uintptr_t get_start()
	{
		return _start;
	}

	uintptr_t get_end()
	{
		return _end;
	}

	uint32_t get_protection()
	{
		//return PAGE_READWRITE;
		return _protection;
	}

	uint32_t get_image()
	{
		return _fImage;
	}

	uint32_t get_page_file()
	{
		return _fPageFile;
	}

	uint32_t get_private_memory()
	{
		return _fPrivateMemory;
	}

	uint32_t get_teb()
	{
		return _fTeb;
	}

	uint32_t get_stack()
	{
		return _fStack;
	}

	uint32_t get_spare()
	{
		return _fSpare;
	}

	uint32_t get_heap_num()
	{
		return _HeapNum;
	}

	uint32_t get_heap()
	{
		return _fHeap;
	}

	uint32_t get_cwsz_description()
	{
		return _cwszDescription;
	}

	uint32_t get_commit_charge()
	{
		return _commitCharge;
	}

	uint32_t get_mem_commit()
	{
		return _memCommit;
	}

	uint32_t get_type()
	{
		switch (_type)
		{
		case 2:
			return MEM_IMAGE;
		case 1:
			return MEM_MAPPED;
		default:
			return MEM_PRIVATE;
		}
	}

	uint32_t get_state()
	{
		if (_fPrivateMemory)
			return MEM_COMMIT;
		else if (_fFile || _fImage)
			return MEM_RESERVE;
		else
			return MEM_FREE;
	}

	VMMDLL_MAP_VADENTRY get_vad()
	{
		return _vad;
	}
};
