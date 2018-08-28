/*
	Highly inefficient and boring interpreter. Nothing special here
*/

#include "types.h"

#include <set>
#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_rom.h"
#include "../sh4_if.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/aica/aica_if.h"
#include "../modules/dmac.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/maple/maple_if.h"
#include "../sh4_interrupts.h"
#include "../modules/tmu.h"
#include "hw/sh4/sh4_mem.h"
#include "../modules/ccn.h"
#include "../dyna/blockmanager.h"
#include "../sh4_sched.h"

#include <time.h>
#include <float.h>

#define SH4_TIMESLICE (448)
#define CPU_RATIO      (8)

//448 Cycles (fixed)
int UpdateSystem(void)
{
	//this is an optimisation (mostly for ARM)
	//makes scheduling easier !
	//update_fp* tmu=pUpdateTMU;
	
	Sh4cntx.sh4_sched_next-=448;
	if (Sh4cntx.sh4_sched_next<0)
		sh4_sched_tick(448);

	return Sh4cntx.interrupt_pend;
}

int UpdateSystem_INTC(void)
{
	UpdateSystem();
	return UpdateINTC();
}

void Sh4_int_Stop(void)
{
	if (sh4_int_bCpuRun)
		sh4_int_bCpuRun=false;
}

void Sh4_int_Run(void)
{
	sh4_int_bCpuRun=true;

	s32 l=SH4_TIMESLICE;

#if !defined(NO_MMU)
   if (settings.MMUEnabled)
   {
      for (int i=0; i<10000; i++)
      {
         try
         {
            do
            {
               u32 addr = next_pc;
               next_pc += 2;
               u32 op = ReadMem16(addr);

               OpPtr[op](op);
               l -= CPU_RATIO;
            } while (l > 0);
            l += SH4_TIMESLICE;
            UpdateSystem_INTC();
         }
         catch (SH4ThrownException ex)
         {
            Do_Exception(ex.epc, ex.expEvn, ex.callVect);
            l -= CPU_RATIO * 5;
         }
      }
   }
   else
#endif
   {
      for (int i=0; i<10000; i++)
      {
         do
         {
            u32 addr = next_pc;
            next_pc += 2;
            u32 op = ReadMem16(addr);

            OpPtr[op](op);
            l -= CPU_RATIO;
         } while (l > 0);
         l += SH4_TIMESLICE;
         UpdateSystem_INTC();
      }
   }
   sh4_int_bCpuRun=false;
}


void Sh4_int_Step(void)
{
	if (sh4_int_bCpuRun)
	{
		printf("Sh4 Is running , can't step\n");
	}
	else
	{
		u32 op=ReadMem16(next_pc);
		next_pc+=2;
		ExecuteOpcode(op);
	}
}

void Sh4_int_Skip(void)
{
	if (!sh4_int_bCpuRun)
		next_pc+=2;
}

void Sh4_int_Reset(bool Manual)
{
   if (sh4_int_bCpuRun)
      return;

   next_pc = 0xA0000000;

   memset(r,0,sizeof(r));
   memset(r_bank,0,sizeof(r_bank));

   gbr=ssr=spc=sgr=dbr=vbr=0;
   mac.full=pr=fpul=0;

   sh4_sr_SetFull(0x700000F0);
   old_sr.status=sr.status;
   UpdateSR();

   fpscr.full = 0x0004001;
   old_fpscr=fpscr;
   UpdateFPSCR();

   //Any more registers have default value ?
   printf("Sh4 Reset\n");
}

bool Sh4_int_IsCpuRunning(void)
{
	return sh4_int_bCpuRun;
}

//TODO : Check for valid delayslot instruction
void ExecuteDelayslot(void)
{
#if !defined(NO_MMU)
   if (settings.MMUEnabled)
   {
      try {
         u32 addr = next_pc;
         next_pc += 2;
         u32 op = IReadMem16(addr);
         if (op != 0)
            ExecuteOpcode(op);
      }
      catch (SH4ThrownException ex)
      {
         ex.epc -= 2;
         //printf("Delay slot exception\n");
         throw ex;
      }
   }
   else
#endif
   {
      u32 addr = next_pc;
      next_pc += 2;
      u32 op = IReadMem16(addr);
      if (op != 0)
         ExecuteOpcode(op);
   }
}

void ExecuteDelayslot_RTE(void)
{
	u32 oldsr = sh4_sr_GetFull();

#if !defined(NO_MMU)
   if (settings.MMUEnabled)
   {
      try {
    	 sh4_sr_SetFull(ssr);
         ExecuteDelayslot();
      }
      catch (SH4ThrownException ex)
      {
         printf("RTE Exception\n");
      }
   }
   else
#endif
   {
	  sh4_sr_SetFull(ssr);
      ExecuteDelayslot();
   }
}

//General update

//3584 Cycles
#define AICA_SAMPLE_GCM 441
#define AICA_SAMPLE_CYCLES (SH4_MAIN_CLOCK/(44100/AICA_SAMPLE_GCM)*32)

int aica_sched;
int rtc_sched;

//14336 Cycles

const int AICA_TICK=145124;

static int AicaUpdate(int tag, int c, int j)
{
   UpdateArm(512*32);
   UpdateAica(1*32);

	return AICA_TICK;
}

static int DreamcastSecond(int tag, int c, int j)
{
	settings.dreamcast.RTC++;
#if FEAT_SHREC != DYNAREC_NONE
	bm_Periodical_1s();
#endif

	//printf("%d ticks\n",sh4_sched_intr);
	sh4_sched_intr=0;
	return SH4_MAIN_CLOCK;
}


static void sh4_int_resetcache(void)
{
}

//Get an interface to sh4 interpreter
void Get_Sh4Interpreter(sh4_if* rv)
{
	rv->Run=Sh4_int_Run;
	rv->Stop=Sh4_int_Stop;
	rv->Step=Sh4_int_Step;
	rv->Skip=Sh4_int_Skip;
	rv->Reset=Sh4_int_Reset;
	rv->Init=Sh4_int_Init;
	rv->Term=Sh4_int_Term;
	rv->IsCpuRunning=Sh4_int_IsCpuRunning;

	rv->ResetCache=sh4_int_resetcache;
}

void Sh4_int_Init()
{
	verify(sizeof(Sh4cntx)==448);

	aica_sched=sh4_sched_register(0,&AicaUpdate);
	sh4_sched_request(aica_sched,AICA_TICK);

	rtc_sched=sh4_sched_register(0,&DreamcastSecond);
	sh4_sched_request(rtc_sched,SH4_MAIN_CLOCK);
	memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));
}

void Sh4_int_Term(void)
{
	Sh4_int_Stop();
	printf("Sh4 Term\n");
}
