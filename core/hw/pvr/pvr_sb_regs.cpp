/*
	PVR-SB handling
	DMA hacks are here
*/

#include "types.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/modules/dmac.h"
#include "hw/sh4/sh4_mem.h"
#include "pvr_sb_regs.h"
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "ta.h"

void RegWrite_SB_C2DST(u32 addr, u32 data)
{
	if(data & 1)
	{
		SB_C2DST=1;
		DMAC_Ch2St();
	}
}

/* PVR-DMA */
void do_pvr_dma(void)
{
	u32 chcr   = DMAC_CHCR(0).full;
	u32 dmaor  = DMAC_DMAOR.full;
	u32 dmatcr = DMAC_DMATCR(0);

	u32 src    = SB_PDSTAR; // System RAM address
	u32 dst    = SB_PDSTAP; // VRAM address
	u32 len    = SB_PDLEN;

	if((dmaor &DMAOR_MASK) != 0x8201)
	{
		printf("\n!\tDMAC: DMAOR has invalid settings (%X) !\n", dmaor);
		return;
	}

	if (len & 0x1F)
	{
		printf("\n!\tDMAC: SB_C2DLEN has invalid size (%X) !\n", len);
		return;
	}

	if (SB_PDDIR)
	{
		//PVR -> System
      WriteMemBlock_nommu_dma(src, dst, len);
	}
	else
	{
		//System -> PVR
      WriteMemBlock_nommu_dma(dst,src,len);
	}

	DMAC_SAR(0)        = (src + len);
	DMAC_CHCR(0).full &= 0xFFFFFFFE;
	DMAC_DMATCR(0)     = 0x00000000;

	SB_PDST            = 0x00000000;

	//TODO : *CHECKME* is that ok here ? the docs don't say here it's used [PVR-DMA , bit 11]
	asic_RaiseInterrupt(holly_PVR_DMA);
}

void RegWrite_SB_PDST(u32 addr, u32 data)
{
	if (data & 1)
	{
		SB_PDST=1;
		do_pvr_dma();
	}
}

u32 calculate_start_link_addr(void)
{
	u32 rv;
	u8* base=&mem_b.data[SB_SDSTAW & RAM_MASK];

	if (SB_SDWLT==0) /* 16b width */
		rv=((u16*)base)[SB_SDDIV];
	else /* 32b width */
		rv=((u32*)base)[SB_SDDIV];

	SB_SDDIV++; //next index

	return rv;
}

void pvr_do_sort_dma(void)
{
	SB_SDDIV           = 0; //index is 0 now :)
	u32 link_addr      = calculate_start_link_addr();
	u32 link_base_addr = SB_SDBAAW;

	while (link_addr!=1)
	{
		if (SB_SDLAS==1)
			link_addr   *= 32;

		u32 ea          = (link_base_addr+link_addr) & RAM_MASK;
		u32* ea_ptr     = (u32*)&mem_b.data[ea];
		link_addr       = ea_ptr[0x1C>>2];//Next link

		/* transfer global param */
		ta_vtx_data(ea_ptr,ea_ptr[0x18>>2]);
		if (link_addr==2)
			link_addr    = calculate_start_link_addr();
	}

	// End of DMA :)
	SB_SDST            = 0;
	asic_RaiseInterrupt(holly_PVR_SortDMA);
}

// Auto sort DMA :|
void RegWrite_SB_SDST(u32 addr, u32 data)
{
	if(data & 1)
		pvr_do_sort_dma();
}

//Init/Term , global
void pvr_sb_Init(void)
{
	//0x005F7C18    SB_PDST RW  PVR-DMA start
	sb_rio_register(SB_PDST_addr,RIO_WF,0,&RegWrite_SB_PDST);

	//0x005F6808    SB_C2DST RW  ch2-DMA start 
	sb_rio_register(SB_C2DST_addr,RIO_WF,0,&RegWrite_SB_C2DST);

	//0x005F6820    SB_SDST RW  Sort-DMA start
	sb_rio_register(SB_SDST_addr,RIO_WF,0,&RegWrite_SB_SDST);
}

void pvr_sb_Term(void)
{
}

//Reset -> Reset - Initialise
void pvr_sb_Reset(bool Manual)
{
}
