/*
	Sh4 internal register routing (P4 & 'area 7')
*/
#include "types.h"
#include "sh4_mmr.h"

#include "hw/mem/_vmem.h"
#include "modules/mmu.h"
#include "modules/ccn.h"
#include "modules/modules.h"

//64bytes of sq // now on context ~

Array<u8> OnChipRAM;

//All registers are 4 byte aligned

Array<RegisterStruct> CCN(18,true);  //CCN  : 16 registers
Array<RegisterStruct> UBC(9,true);   //UBC  : 9 registers
Array<RegisterStruct> BSC(19,true);  //BSC  : 18 registers
Array<RegisterStruct> DMAC(17,true); //DMAC : 17 registers
Array<RegisterStruct> CPG(5,true);   //CPG  : 5 registers
Array<RegisterStruct> RTC(16,true);  //RTC  : 16 registers
Array<RegisterStruct> INTC(5,true);  //INTC : 5 registers
Array<RegisterStruct> TMU(12,true);  //TMU  : 12 registers
Array<RegisterStruct> SCI(8,true);   //SCI  : 8 registers
Array<RegisterStruct> SCIF(10,true); //SCIF : 10 registers

Array<RegisterStruct> * const AllRegisters[] = { &CCN, &UBC, &BSC, &DMAC, &CPG, &RTC, &INTC, &TMU, &SCI, &SCIF };

u32 sh4io_read_noacc(u32 addr) 
{ 
	INFO_LOG(SH4, "sh4io: Invalid read access @@ %08X", addr);
	return 0; 
} 

void sh4io_write_noacc(u32 addr, u32 data) 
{ 
	INFO_LOG(SH4, "sh4io: Invalid write access @@ %08X %08X", addr, data);
	//verify(false); 
}

void sh4io_write_const(u32 addr, u32 data) 
{ 
	INFO_LOG(SH4, "sh4io: Const write ignored @@ %08X <- %08X", addr, data);
}

void sh4_rio_reg(Array<RegisterStruct>& arr, u32 addr, RegIO flags, u32 sz, RegReadAddrFP* rf, RegWriteAddrFP* wf)
{
	u32 idx=(addr&255)/4;

	verify(idx<arr.Size);

	arr.data[idx].flags = flags | REG_ACCESS_32;

   if (flags == RIO_NO_ACCESS)
   {
      arr.data[idx].readFunctionAddr=&sh4io_read_noacc;
      arr.data[idx].writeFunctionAddr=&sh4io_write_noacc;
   }
   else if (flags == RIO_CONST)
   {
      arr.data[idx].writeFunctionAddr=&sh4io_write_const;
   }
   else
   {
      arr.data[idx].data32=0;

      if (flags & REG_RF)
         arr.data[idx].readFunctionAddr=rf;

      if (flags & REG_WF)
         arr.data[idx].writeFunctionAddr=wf==0?&sh4io_write_noacc:wf;
   }
}

template<u32 sz>
u32 sh4_rio_read(Array<RegisterStruct>& sb_regs, u32 addr)
{	
   u32 offset = addr&255;
#ifdef TRACE
	if (offset & 3/*(size-1)*/) //4 is min align size
	{
		INFO_LOG(SH4, "Unaligned System Bus register read");
	}
#endif

   offset>>=2;

#ifdef TRACE
	if (sb_regs[offset].flags & sz)
	{
#endif
      if (!(sb_regs.data[offset].flags & REG_RF) )
		{
			if (sz==4)
				return  sb_regs.data[offset].data32;
			else if (sz==2)
				return  sb_regs.data[offset].data16;
			else 
				return  sb_regs.data[offset].data8;
		}
		else
		{
			return sb_regs.data[offset].readFunctionAddr(addr);
		}
#ifdef TRACE
	}
	else
	{
		INFO_LOG(SH4, "ERROR [wrong size read on register]");
	}
#endif
	return 0;
}

template<u32 sz>
void sh4_rio_write(Array<RegisterStruct>& sb_regs, u32 addr, u32 data)
{
	u32 offset = addr&255;
#ifdef TRACE
	if (offset & 3/*(size-1)*/) //4 is min align size
	{
		INFO_LOG(SH4, "Unaligned System bus register write");
	}
#endif
offset>>=2;
#ifdef TRACE
	if (sb_regs[offset].flags & sz)
	{
#endif
		if (!(sb_regs.data[offset].flags & REG_WF) )
		{
			if (sz==4)
				sb_regs.data[offset].data32=data;
			else if (sz==2)
				sb_regs.data[offset].data16=(u16)data;
			else
				sb_regs.data[offset].data8=(u8)data;
			return;
		}
		else
		{
			//printf("RSW: %08X\n",addr);
			sb_regs[offset].writeFunctionAddr(addr,data);
			return;
		}
#ifdef TRACE
	}
	else
	{
		INFO_LOG(SH4, "ERROR: Wrong size write on register - offset=%x, data=%x, size=%d",offset,data,sz);
	}
#endif
}

//Region P4
//Read P4
template <u32 sz,class T>
T DYNACALL ReadMem_P4(u32 addr)
{
	switch((addr>>24)&0xFF)
   {
      case 0xE0:
      case 0xE1:
      case 0xE2:
      case 0xE3:
		INFO_LOG(SH4, "Unhandled p4 read [Store queue] 0x%x", addr);
         return 0;
      case 0xF0:
         return 0;
      case 0xF1:
         return 0;
      case 0xF2:
         {
            u32 entry=(addr>>8)&3;
            return ITLB[entry].Address.reg_data | (ITLB[entry].Data.V<<8);
         }

      case 0xF3:
         {
            u32 entry=(addr>>8)&3;
            return ITLB[entry].Data.reg_data;
         }

      case 0xF4:
#if 0
         {
            int W,Set,A;
            W=(addr>>14)&1;
            A=(addr>>3)&1;
            Set=(addr>>5)&0xFF;
            printf("Unhandled p4 read [Operand cache address array] %d:%d,%d  0x%x\n",Set,W,A,addr);
         }
#endif
         return 0;
      case 0xF5:
         return 0;
      case 0xF6:
         {
            u32 entry=(addr>>8)&63;
            u32 rv=UTLB[entry].Address.reg_data;
            rv|=UTLB[entry].Data.D<<9;
            rv|=UTLB[entry].Data.V<<8;
            return rv;
         }

      case 0xF7:
         {
            u32 entry=(addr>>8)&63;
            return UTLB[entry].Data.reg_data;
         }

      case 0xFF:
		INFO_LOG(SH4, "Unhandled p4 read [area7] 0x%x", addr);
         break;

      default:
		INFO_LOG(SH4, "Unhandled p4 read [Reserved] 0x%x", addr);
         break;
   }

	return 0;

}

//Write P4
template <u32 sz,class T>
void DYNACALL WriteMem_P4(u32 addr,T data)
{
   /*if (((addr>>26)&0x7)==7)
     {
     WriteMem_area7(addr,data,sz);
     return;
     }*/

   switch((addr>>24)&0xFF)
   {
      case 0xE0:
      case 0xE1:
      case 0xE2:
      case 0xE3:
		INFO_LOG(SH4, "Unhandled p4 Write [Store queue] 0x%x", addr);
         break;

      case 0xF0:
         return;
      case 0xF1:
         return;
      case 0xF2:
         {
            u32 entry=(addr>>8)&3;
            ITLB[entry].Address.reg_data=data & 0xFFFFFCFF;
            ITLB[entry].Data.V=(data>>8) & 1;
            ITLB_Sync(entry);
         }
         return;
      case 0xF3:
      {
      	u32 entry=(addr>>8)&3;
      	if (addr&0x800000)
      	{
      		ITLB[entry].Assistance.reg_data = data & 0xf;
      	}
      	else
      	{
      		ITLB[entry].Data.reg_data=data;
      	}
      	ITLB_Sync(entry);
      	return;
      }

      case 0xF4:
#if 0
         {
            int W,Set,A;
            W=(addr>>14)&1;
            A=(addr>>3)&1;
            Set=(addr>>5)&0xFF;
            printf("Unhandled p4 Write [Operand cache address array] %d:%d,%d  0x%x = %x\n",Set,W,A,addr,data);
         }
#endif
         return;
      case 0xF5:
		//printf("Unhandled p4 Write [Operand cache data array] 0x%x = %x\n",addr,data);
         return;
      case 0xF6:
         {
            if (addr&0x80)
            {
#ifdef NO_MMU
               INFO_LOG(SH4, "Unhandled p4 Write [Unified TLB address array, Associative Write] 0x%x = %x", addr, data);
#endif

               CCN_PTEH_type t;
               t.reg_data=data;

               u32 va=t.VPN<<10;

#ifndef NO_MMU
					for (int i=0;i<64;i++)
					{
						if (mmu_match(va,UTLB[i].Address,UTLB[i].Data))
						{
							UTLB[i].Data.V=((u32)data>>8)&1;
							UTLB[i].Data.D=((u32)data>>9)&1;
							UTLB_Sync(i);
						}
					}

					for (int i=0;i<4;i++)
					{
						if (mmu_match(va,ITLB[i].Address,ITLB[i].Data))
						{
							ITLB[i].Data.V=((u32)data>>8)&1;
							ITLB[i].Data.D=((u32)data>>9)&1;
							ITLB_Sync(i);
						}
					}
#endif
            }
            else
            {
               u32 entry=(addr>>8)&63;
               UTLB[entry].Address.reg_data=data & 0xFFFFFCFF;
               UTLB[entry].Data.D=(data>>9)&1;
               UTLB[entry].Data.V=(data>>8)&1;
               UTLB_Sync(entry);
            }
            return;
         }
         break;

      case 0xF7:
      {
      	u32 entry=(addr>>8)&63;
      	if (addr&0x800000)
      	{
      		UTLB[entry].Assistance.reg_data = data & 0xf;
      	}
      	else
      	{
      		UTLB[entry].Data.reg_data=data;
      	}
      	UTLB_Sync(entry);

      	return;
      }

      case 0xFF:
		INFO_LOG(SH4, "Unhandled p4 Write [area7] 0x%x = %x", addr, data);
         break;

      default:
		INFO_LOG(SH4, "Unhandled p4 Write [Reserved] 0x%x", addr);
         break;
   }
}


//***********
//Store Queue
//***********
//TODO : replace w/ mem mapped array
//Read SQ
template <u32 sz,class T>
T DYNACALL ReadMem_sq(u32 addr)
{
	if (sz!=4)
	{
		INFO_LOG(SH4, "Store Queue Error - only 4 byte read are possible[x%X]", addr);
		return 0xDE;
	}

	u32 united_offset=addr & 0x3C;

	return (T)*(u32*)&sq_both[united_offset];
}


//Write SQ
template <u32 sz,class T>
void DYNACALL WriteMem_sq(u32 addr,T data)
{
	if (sz!=4)
		INFO_LOG(SH4, "Store Queue Error - only 4 byte writes are possible[x%X=0x%X]", addr, data);

	u32 united_offset=addr & 0x3C;

	*(u32*)&sq_both[united_offset]=data;
}


//***********
//**Area  7**
//***********

#define OUT_OF_RANGE(reg) INFO_LOG(SH4, "Out of range on register %s index %x", reg, addr)

//Read Area7
template <u32 sz,class T>
T DYNACALL ReadMem_area7(u32 addr)
{
	/*
	if (likely(addr==0xffd80024))
	{
		return TMU_TCNT(2);
	}
	else if (likely(addr==0xFFD8000C))
	{
		return TMU_TCNT(0);
	}
	else */if (likely(addr==0xFF000028))
	{
		return CCN_INTEVT;
	}
	else if (likely(addr==0xFFA0002C))
	{
		return DMAC_CHCR(2).full;
	}
	//else if (addr==)

	//printf("%08X\n",addr);
	addr&=0x1FFFFFFF;
	u32 map_base=addr>>16;
	switch (map_base & 0x1FFF)
	{
	case A7_REG_HASH(CCN_BASE_addr):
		if (addr<=0x1F000044)
		{
			return (T)sh4_rio_read<sz>(CCN,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("CCN");
			return 0;
		}
		break;

	case A7_REG_HASH(UBC_BASE_addr):
		if (addr<=0x1F200020)
		{
			return (T)sh4_rio_read<sz>(UBC,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("UBC");
			return 0;
		}
		break;

	case A7_REG_HASH(BSC_BASE_addr):
		if (addr<=0x1F800048)
		{
			return (T)sh4_rio_read<sz>(BSC,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("BSC");
			return 0;
		}
		break;
	case A7_REG_HASH(BSC_SDMR2_addr):
		//dram settings 2 / write only
		INFO_LOG(SH4, "Read from write-only registers [dram settings 2]");
		return 0;
	case A7_REG_HASH(BSC_SDMR3_addr):
		//dram settings 3 / write only
		INFO_LOG(SH4, "Read from write-only registers [dram settings 3]");
		return 0;



	case A7_REG_HASH(DMAC_BASE_addr):
		if (addr<=0x1FA00040)
		{
			return (T)sh4_rio_read<sz>(DMAC,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("DMAC");
			return 0;
		}
		break;

	case A7_REG_HASH(CPG_BASE_addr):
		if (addr<=0x1FC00010)
		{
			return (T)sh4_rio_read<sz>(CPG,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("CPG");
			return 0;
		}
		break;

	case A7_REG_HASH(RTC_BASE_addr):
		if (addr<=0x1FC8003C)
		{
			return (T)sh4_rio_read<sz>(RTC,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("RTC");
			return 0;
		}
		break;

	case A7_REG_HASH(INTC_BASE_addr):
		if (addr<=0x1FD00010)
		{
			return (T)sh4_rio_read<sz>(INTC,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("INTC");
			return 0;
		}
		break;

	case A7_REG_HASH(TMU_BASE_addr):
		if (addr<=0x1FD8002C)
		{
			return (T)sh4_rio_read<sz>(TMU,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("TMU");
			return 0;
		}
		break;

	case A7_REG_HASH(SCI_BASE_addr):
		if (addr<=0x1FE0001C)
		{
			return (T)sh4_rio_read<sz>(SCI,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("SCI");
			return 0;
		}
		break;

	case A7_REG_HASH(SCIF_BASE_addr):
		if (addr<=0x1FE80024)
		{
			return (T)sh4_rio_read<sz>(SCIF,addr & 0xFF);
		}
		else
		{
			OUT_OF_RANGE("SCIF");
			return 0;
		}
		break;

		// Who really cares about ht-UDI? it's not existent on the Dreamcast IIRC
	case A7_REG_HASH(UDI_BASE_addr):
		switch(addr)
		{
			//UDI SDIR 0x1FF00000 0x1FF00000 16 0xFFFF Held Held Held Pclk
		case UDI_SDIR_addr :
			break;


			//UDI SDDR 0x1FF00008 0x1FF00008 32 Held Held Held Held Pclk
		case UDI_SDDR_addr :
			break;
		}
		break;
	}


	INFO_LOG(SH4, "Unknown Read from Area7 - addr=%x", addr);
	return 0;
}

//Write Area7
template <u32 sz,class T>
void DYNACALL WriteMem_area7(u32 addr,T data)
{
	if (likely(addr==0xFF000038))
	{
		CCN_QACR_write<0>(addr,data);
		return;
	}
	else if (likely(addr==0xFF00003C))
	{
		CCN_QACR_write<1>(addr,data);
		return;
	}	

	//printf("%08X\n",addr);

	addr&=0x1FFFFFFF;
	u32 map_base=addr>>16;
	switch (map_base & 0x1FFF)
	{

	case A7_REG_HASH(CCN_BASE_addr):
		if (addr<=0x1F00003C)
		{
			sh4_rio_write<sz>(CCN,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("CCN");
		}
		return;

	case A7_REG_HASH(UBC_BASE_addr):
		if (addr<=0x1F200020)
		{
			sh4_rio_write<sz>(UBC,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("UBC");
		}
		return;

	case A7_REG_HASH(BSC_BASE_addr):
		if (addr<=0x1F800048)
		{
			sh4_rio_write<sz>(BSC,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("BSC");
		}
		return;
	case A7_REG_HASH(BSC_SDMR2_addr):
		//dram settings 2 / write only
		return;

	case A7_REG_HASH(BSC_SDMR3_addr):
		//dram settings 3 / write only
		return;


	case A7_REG_HASH(DMAC_BASE_addr):
		if (addr<=0x1FA00040)
		{
			sh4_rio_write<sz>(DMAC,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("DMAC");
		}
		return;

	case A7_REG_HASH(CPG_BASE_addr):
		if (addr<=0x1FC00010)
		{
			sh4_rio_write<sz>(CPG,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("CPG");
		}
		return;

	case A7_REG_HASH(RTC_BASE_addr):
		if (addr<=0x1FC8003C)
		{
			sh4_rio_write<sz>(RTC,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("RTC");
		}
		return;

	case A7_REG_HASH(INTC_BASE_addr):
		if (addr<=0x1FD0000C)
		{
			sh4_rio_write<sz>(INTC,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("INTC");
		}
		return;

	case A7_REG_HASH(TMU_BASE_addr):
		if (addr<=0x1FD8002C)
		{
			sh4_rio_write<sz>(TMU,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("TMU");
		}
		return;

	case A7_REG_HASH(SCI_BASE_addr):
		if (addr<=0x1FE0001C)
		{
			sh4_rio_write<sz>(SCI,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("SCI");
		}
		return;

	case A7_REG_HASH(SCIF_BASE_addr):
		if (addr<=0x1FE80024)
		{
			sh4_rio_write<sz>(SCIF,addr & 0xFF,data);
		}
		else
		{
			OUT_OF_RANGE("SCIF");
		}
		return;

		//who really cares about ht-udi ? it's not existent on dc iirc ..
	case A7_REG_HASH(UDI_BASE_addr):
		switch(addr)
		{
			//UDI SDIR 0xFFF00000 0x1FF00000 16 0xFFFF Held Held Held Pclk
		case UDI_SDIR_addr :
			break;


			//UDI SDDR 0xFFF00008 0x1FF00008 32 Held Held Held Held Pclk
		case UDI_SDDR_addr :
			break;
		}
		break;
	}

	INFO_LOG(SH4, "Write to Area7 not implemented, addr=%x, data=%x", addr, data);
}



//***********
//On Chip Ram
//***********
//Read OCR
template <u32 sz,class T>
T DYNACALL ReadMem_area7_OCR_T(u32 addr)
{
   if (CCN_CCR.ORA)
   {
      if (sz==1)
            return (T)OnChipRAM.data[addr&OnChipRAM_MASK];
      else if (sz==2)
            return (T)*(u16*)&OnChipRAM.data[addr&OnChipRAM_MASK];
      else if (sz==4)
            return (T)*(u32*)&OnChipRAM.data[addr&OnChipRAM_MASK];
      else
      {
			ERROR_LOG(SH4, "ReadMem_area7_OCR_T: template SZ is wrong = %d", sz);
            return 0xDE;
      }
   }
   else
	{
		INFO_LOG(SH4, "On Chip Ram Read, but OCR is disabled");
		return 0xDE;
	}
}

//Write OCR
template <u32 sz,class T>
void DYNACALL WriteMem_area7_OCR_T(u32 addr,T data)
{
   if (CCN_CCR.ORA)
   {
      if (sz==1)
         OnChipRAM.data[addr&OnChipRAM_MASK]=(u8)data;
      else if (sz==2)
         *(u16*)&OnChipRAM.data[addr&OnChipRAM_MASK]=(u16)data;
      else if (sz==4)
         *(u32*)&OnChipRAM.data[addr&OnChipRAM_MASK]=data;
      else
      {
			ERROR_LOG(SH4, "WriteMem_area7_OCR_T: template SZ is wrong = %d", sz);
      }
   }
   else
   {
		INFO_LOG(SH4, "On Chip Ram Write, but OCR is disabled");
	}
}


//Init/Res/Term
void sh4_mmr_init(void)
{
	OnChipRAM.Resize(OnChipRAM_SIZE,false);

	for (u32 i=0;i<30;i++)
	{
		if (i<CCN.Size)  sh4_rio_reg(CCN,CCN_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(16,true);    //CCN  : 14 registers
		if (i<UBC.Size)  sh4_rio_reg(UBC,UBC_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(9,true);     //UBC  : 9 registers
		if (i<BSC.Size)  sh4_rio_reg(BSC,BSC_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(19,true);    //BSC  : 18 registers
		if (i<DMAC.Size) sh4_rio_reg(DMAC,DMAC_BASE_addr+i*4,RIO_NO_ACCESS,32); //(17,true);    //DMAC : 17 registers
		if (i<CPG.Size)  sh4_rio_reg(CPG,CPG_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(5,true);     //CPG  : 5 registers
		if (i<RTC.Size)  sh4_rio_reg(RTC,RTC_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(16,true);    //RTC  : 16 registers
		if (i<INTC.Size) sh4_rio_reg(INTC,INTC_BASE_addr+i*4,RIO_NO_ACCESS,32); //(4,true);     //INTC : 4 registers
		if (i<TMU.Size)  sh4_rio_reg(TMU,TMU_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(12,true);    //TMU  : 12 registers
		if (i<SCI.Size)  sh4_rio_reg(SCI,SCI_BASE_addr+i*4,RIO_NO_ACCESS,32);   //(8,true);     //SCI  : 8 registers
		if (i<SCIF.Size) sh4_rio_reg(SCIF,SCIF_BASE_addr+i*4,RIO_NO_ACCESS,32); //(10,true);    //SCIF : 10 registers
	}

	//initialise Register structs
	bsc_init();
	ccn_init();
	cpg_init();
	dmac_init();
	intc_init();
	rtc_init();
	serial_init();
	tmu_init();
	ubc_init();
}

void sh4_mmr_reset(bool hard)
{
	if (hard)
	{
		for (int i = 0; i < ARRAY_SIZE(AllRegisters); i++)
			for (int j = 0; j < AllRegisters[i]->Size; j++)
				(*AllRegisters[i])[j].reset();
	}
	OnChipRAM.Zero();
	//Reset register values
	bsc_reset(hard);
	ccn_reset(hard);
	cpg_reset();
	dmac_reset();
	intc_reset();
	rtc_reset();
	serial_reset();
	tmu_reset(hard);
	ubc_reset();
}

void sh4_mmr_term(void)
{
	//free any alloc'd resources [if any]
	ubc_term();
	tmu_term();
	serial_term();
	rtc_term();
	intc_term();
	dmac_term();
	cpg_term();
	ccn_term();
	bsc_term();
	OnChipRAM.Free();
}
//Mem map :)

//AREA 7--Sh4 Regs
_vmem_handler area7_handler;

_vmem_handler area7_orc_handler;

void map_area7_init(void)
{
	//=_vmem_register_handler(ReadMem8_area7,ReadMem16_area7,ReadMem32_area7,
	//									WriteMem8_area7,WriteMem16_area7,WriteMem32_area7);

	//default area7 handler
	area7_handler= _vmem_register_handler_Template(ReadMem_area7,WriteMem_area7);

	area7_orc_handler= _vmem_register_handler_Template(ReadMem_area7_OCR_T,WriteMem_area7_OCR_T);
}

void map_area7(u32 base)
{
	//OCR @
	//((addr>=0x7C000000) && (addr<=0x7FFFFFFF))
	if (base==0x60)
		_vmem_map_handler(area7_orc_handler, 0x1C | base , 0x1F | base);
	else
	{
		_vmem_map_handler(area7_handler,     0x1C | base , 0x1F | base);
	}
}

//P4
void map_p4(void)
{
	//P4 Region :
	_vmem_handler p4_handler = _vmem_register_handler_Template(ReadMem_P4,WriteMem_P4);

	//register this before area7 and SQ , so they overwrite it and handle em :)
	//default P4 handler
	//0xE0000000-0xFFFFFFFF
	_vmem_map_handler(p4_handler,0xE0,0xFF);

	//Store Queues -- Write only 32bit
	_vmem_map_block(sq_both,0xE0,0xE0,63);
	_vmem_map_block(sq_both,0xE1,0xE1,63);
	_vmem_map_block(sq_both,0xE2,0xE2,63);
	_vmem_map_block(sq_both,0xE3,0xE3,63);

	map_area7(0xE0);
}
