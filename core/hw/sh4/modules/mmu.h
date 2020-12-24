#pragma once
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/mem/_vmem.h"

//Translation Types
//Opcode read
#define MMU_TT_IREAD 0
//Data write
#define MMU_TT_DWRITE 1
//Data write
#define MMU_TT_DREAD 2

//Return Values
//Translation was successful
#define MMU_ERROR_NONE	   0
//TLB miss
#define MMU_ERROR_TLB_MISS 1
//TLB Multihit
#define MMU_ERROR_TLB_MHIT 2
//Mem is read/write protected (depends on translation type)
#define MMU_ERROR_PROTECTED 3
//Mem is write protected , firstwrite
#define MMU_ERROR_FIRSTWRITE 4
//data-Opcode read/write missasligned
#define MMU_ERROR_BADADDR 5
//Can't Execute
#define MMU_ERROR_EXECPROT 6

struct TLB_Entry
{
	CCN_PTEH_type Address;
	CCN_PTEL_type Data;
	CCN_PTEA_type Assistance;
};

extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
extern u32 sq_remap[64];
extern const u32 fast_reg_lut[8];

//These are working only for SQ remaps on ndce
bool UTLB_Sync(u32 entry);
void ITLB_Sync(u32 entry);

bool mmu_match(u32 va, CCN_PTEH_type Address, CCN_PTEL_type Data);
void mmu_set_state();
void mmu_flush_table();
void mmu_raise_exception(u32 mmu_error, u32 address, u32 am);

static INLINE bool mmu_enabled()
{
#ifndef NO_MMU
	return settings.dreamcast.FullMMU && CCN_MMUCR.AT == 1;
#else
	return false;
#endif
}

template<bool internal = false>
u32 mmu_full_lookup(u32 va, const TLB_Entry **entry, u32& rv);

#ifdef FAST_MMU
static INLINE u32 mmu_instruction_translation(u32 va, u32& rv)
{
	if (va & 1)
		return MMU_ERROR_BADADDR;
	if (fast_reg_lut[va >> 29] != 0)
	{
		rv = va;
		return MMU_ERROR_NONE;
	}

	const TLB_Entry *tlb_entry;
	return mmu_full_lookup(va, &tlb_entry, rv);
}
#else
u32 mmu_instruction_translation(u32 va, u32& rv);
#endif

template<u32 translation_type, typename T>
extern u32 mmu_data_translation(u32 va, u32& rv);
void DoMMUException(u32 addr, u32 error_code, u32 access_type);

#if defined(NO_MMU)
	bool inline mmu_TranslateSQW(u32 addr, u32* mapped) {
		*mapped = sq_remap[(addr>>20)&0x3F] | (addr & 0xFFFE0);
		return true;
	}
	void inline mmu_flush_table() {}
#else
	template<typename T> T DYNACALL mmu_ReadMem(u32 adr);
	u16 DYNACALL mmu_IReadMem16(u32 addr);

	template<typename T> void DYNACALL mmu_WriteMem(u32 adr, T data);
	
	bool mmu_TranslateSQW(u32 addr, u32* mapped);

	template<typename T>
	T DYNACALL mmu_ReadMemNoEx(u32 adr, u32 *exception_occurred)
	{
		u32 addr;
		u32 rv = mmu_data_translation<MMU_TT_DREAD, T>(adr, addr);
		if (rv != MMU_ERROR_NONE)
		{
			DoMMUException(adr, rv, MMU_TT_DREAD);
			*exception_occurred = 1;
			return 0;
		}
		else
		{
			*exception_occurred = 0;
			return _vmem_readt<T, T>(addr);
		}
	}

	template<typename T>
	u32 DYNACALL mmu_WriteMemNoEx(u32 adr, T data)
	{
		u32 addr;
		u32 rv = mmu_data_translation<MMU_TT_DWRITE, T>(adr, addr);
		if (rv != MMU_ERROR_NONE)
		{
			DoMMUException(adr, rv, MMU_TT_DWRITE);
			return 1;
		}
		_vmem_writet<T>(addr, data);
		return 0;
	}
#endif
