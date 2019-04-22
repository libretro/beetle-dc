#include "types.h"
#include <string.h>

#include "maple_if.h"

#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_mem.h"
#include "types.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/maple/maple_helper.h"
#include "maple_cfg.h"

enum MaplePattern
{
	MP_Start,
	MP_SDCKBOccupy = 2,
	MP_Reset,
	MP_SDCKBOccupyCancel,
	MP_NOP = 7
};

maple_device* MapleDevices[4][6];

int maple_sched;

/*
	Maple host controller
	Direct processing, async interrupt handling
	Device code is on maple_devs.cpp/h, config&management is on maple_cfg.cpp/h

	This code is missing many of the hardware details, like proper trigger handling,
	DMA continuation on suspect, etc ...
*/

u32 dmacount=0;

static bool IsOnSh4Ram(u32 addr)
{
	if (((addr>>26)&0x7)==3)
	{
		if ((((addr>>29) &0x7)!=7))
			return true;
	}

	return false;
}

static void maple_DoDma(void)
{
	verify(SB_MDEN &1)
	verify(SB_MDST &1)

	u32 addr = SB_MDSTAR;
	u32 xfer_count=0;
	bool last = false;
	while (last != true)
	{
		dmacount++;
		u32 header_1 = ReadMem32_nommu(addr);
		u32 header_2 = ReadMem32_nommu(addr + 4) &0x1FFFFFE0;

		last = (header_1 >> 31) == 1;//is last transfer ?
		u32 plen = (header_1 & 0xFF )+1;//transfer length (32-bit unit)
		u32 maple_op=(header_1>>8)&7;	// Pattern selection: 0 - START, 2 - SDCKB occupy permission, 3 - RESET, 4 - SDCKB occupy cancel, 7 - NOP
		xfer_count+=plen*4;

		//this is kinda wrong .. but meh
		//really need to properly process the commands at some point
		switch (maple_op)
		{
		case MP_Start:
		{
			if (!IsOnSh4Ram(header_2))
			{
				printf("MAPLE ERROR : DESTINATION NOT ON SH4 RAM 0x%X\n",header_2);
				header_2&=0xFFFFFF;
				header_2|=(3<<26);
			}
			u32* p_out=(u32*)GetMemPtr(header_2,4);
			u32 outlen=0;

			u32* p_data =(u32*) GetMemPtr(addr + 8,(plen)*sizeof(u32));
			if (p_data == NULL)
			{
				printf("MAPLE ERROR : INVALID SB_MDSTAR value 0x%X\n", addr);
				SB_MDST=0;
				return;
			}
			
			//Command code 
			u32 command=p_data[0] &0xFF;
			//Recipient address 
			u32 reci=(p_data[0] >> 8) & 0xFF;//0-5;
			u32 port=maple_GetPort(reci);
			u32 bus=maple_GetBusId(reci);
			//Sender address 
			u32 send=(p_data[0] >> 16) & 0xFF;
			//Number of additional words in frame 
			u32 inlen=(p_data[0]>>24) & 0xFF;
			u32 resp=0;
			inlen*=4;

			if (MapleDevices[bus][5] && MapleDevices[bus][port])
			{
				u32 outlen = MapleDevices[bus][port]->RawDma(&p_data[0], inlen + 4, &p_out[0]);
				xfer_count += outlen;
			}
			else
			{
				if (port != 5 && command != 1)
					printf("MAPLE: Unknown device bus %d port %d cmd %d\n", bus, port, command);
				outlen=4;
				p_out[0]=0xFFFFFFFF;
			}

			//goto next command
			addr += 2 * 4 + plen * 4;
		}
		break;

		case MP_SDCKBOccupy:
		{
			u32 bus = (header_1 >> 16) & 3;
			if (MapleDevices[bus][5])
				MapleDevices[bus][5]->get_lightgun_pos();

			addr += 1 * 4;
		}
		break;

		case MP_SDCKBOccupyCancel:
			addr += 1 * 4;
			break;

		case MP_Reset:
			addr += 1 * 4;
			break;

		case MP_NOP:
			addr += 1 * 4;
			break;

		default:
			printf("MAPLE: Unknown maple_op == %d length %d\n", maple_op, plen * 4);
			addr += 1 * 4;
		}
	}

	//printf("Maple XFER size %d bytes - %.2f ms\n",xfer_count,xfer_count*100.0f/(2*1024*1024/8));
	sh4_sched_request(maple_sched,xfer_count*(SH4_MAIN_CLOCK/(2*1024*1024/8)));
}

static void maple_handle_reconnect();

//really hackish
//misses delay , and stop/start implementation
//ddt/etc are just hacked for wince to work
//now with proper maple delayed DMA maybe its time to look into it ?
bool maple_ddt_pending_reset=false;
void maple_vblank()
{
	if (SB_MDEN &1)
	{
		if (SB_MDTSEL&1)
		{
			if (!maple_ddt_pending_reset)
			{
				//printf("DDT vblank\n");
				maple_DoDma();
				SB_MDST = 0;
				if ((SB_MSYS>>12)&1)
				{
					maple_ddt_pending_reset=true;
				}
			}
		}
		else
		{
			maple_ddt_pending_reset=false;
		}
	}
	maple_handle_reconnect();
}
void maple_SB_MSHTCL_Write(u32 addr, u32 data)
{
	if (data&1)
		maple_ddt_pending_reset=false;
}
void maple_SB_MDST_Write(u32 addr, u32 data)
{
	if (data & 0x1)
	{
		if (SB_MDEN &1)
		{
			SB_MDST=1;
			maple_DoDma();
		}
	}
}

void maple_SB_MDEN_Write(u32 addr, u32 data)
{
	SB_MDEN=data&1;
}

int maple_schd(int tag, int c, int j)
{
	if (SB_MDEN&1)
	{
		SB_MDST=0;
		asic_RaiseInterrupt(holly_MAPLE_DMA);
	}
	else
	{
		printf("WARNING: MAPLE DMA ABORT\n");
		SB_MDST=0; //I really wonder what this means, can the DMA be continued ?
	}

	return 0;
}

//Init registers :)
void maple_Init()
{
	sb_rio_register(SB_MDST_addr,RIO_WF,0,&maple_SB_MDST_Write);
	sb_rio_register(SB_MDEN_addr,RIO_WF,0,&maple_SB_MDEN_Write);

	/*
	sb_regs[(SB_MDST_addr-SB_BASE)>>2].flags=REG_32BIT_READWRITE | REG_READ_DATA;
	sb_regs[(SB_MDST_addr-SB_BASE)>>2].writeFunction=maple_SB_MDST_Write;
	*/

	sb_rio_register(SB_MSHTCL_addr,RIO_WF,0,&maple_SB_MSHTCL_Write);
	
	/*
	sb_regs[(SB_MSHTCL_addr-SB_BASE)>>2].flags=REG_32BIT_READWRITE;
	sb_regs[(SB_MSHTCL_addr-SB_BASE)>>2].writeFunction=maple_SB_MSHTCL_Write;
	*/

	maple_sched=sh4_sched_register(0,&maple_schd);
}

void maple_Reset(bool Manual)
{
	maple_ddt_pending_reset=false;
	SB_MDTSEL = 0x00000000;
	SB_MDEN   = 0x00000000;
	SB_MDST   = 0x00000000;
	SB_MSYS   = 0x3A980000;
	SB_MSHTCL = 0x00000000;
	SB_MDAPRO = 0x00007F00;
	SB_MMSEL  = 0x00000001;
}

void maple_Term()
{
	
}

static u64 reconnect_time;

void maple_ReconnectDevices()
{
	mcfg_DestroyDevices();
	reconnect_time = sh4_sched_now64() + SH4_MAIN_CLOCK / 10;
}

static void maple_handle_reconnect()
{
	if (reconnect_time != 0 && reconnect_time <= sh4_sched_now64())
	{
		reconnect_time = 0;
		mcfg_CreateDevices();
	}
}
