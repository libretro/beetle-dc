// nullDC.cpp : Makes magic cookies
//

//initialse Emu
#include "types.h"
#include "hw/mem/_vmem.h"
#include "stdclass.h"

#include "types.h"
#include "hw/flashrom/flashrom.h"
#include "hw/maple/maple_cfg.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/arm7/arm7.h"

#include "hw/naomi/naomi_cart.h"

#include "reios/reios.h"
#include <libretro.h>

extern RomChip sys_rom;
extern SRamChip sys_nvmem_sram;
extern DCFlashChip sys_nvmem_flash;
unsigned FLASH_SIZE;
unsigned BBSRAM_SIZE;
unsigned BIOS_SIZE;
unsigned RAM_SIZE;
unsigned ARAM_SIZE;
unsigned VRAM_SIZE;
unsigned RAM_MASK;
unsigned ARAM_MASK;
unsigned VRAM_MASK;

extern retro_log_printf_t         log_cb;
settings_t settings;

extern char game_dir[1024];
extern char *game_data;
extern bool boot_to_bios;
extern bool reset_requested;

extern void init_mem();
extern void arm_Init();
extern void term_mem();

/*
	libndc

	//initialise (and parse the command line)
	ndc_init(argc,argv);

	...
	//run a dreamcast slice
	//either a frame, or up to 25 ms of emulation
	//returns 1 if the frame is ready (fb needs to be flipped -- i'm looking at you android)
	ndc_step();

	...
	//terminate (and free everything)
	ndc_term()
*/

int GetFile(char *szFileName, char *szParse=0,u32 flags=0) 
{
   if (!boot_to_bios)
   {
      strcpy(szFileName, game_data);
      strcpy(settings.imgread.DefaultImage, szFileName);
   }

	return 1; 
}


s32 plugins_Init(char *s, size_t len)
{
   if (s32 rv = libPvr_Init())
      return rv;

   if (s32 rv = libGDR_Init())
      return rv;
   if (settings.System != DC_PLATFORM_DREAMCAST)
   {
      if (!naomi_cart_SelectFile(s, len))
         return rv_serror;
   }

   if (s32 rv = libAICA_Init())
      return rv;

   init_mem();
   arm_Init();

   //if (s32 rv = libExtDevice_Init())
   //	return rv;



   return rv_ok;
}

void plugins_Term(void)
{
   //term all plugins
   //libExtDevice_Term();
   
	term_mem();
	//arm7_Term ?
   libAICA_Term();

   libGDR_Term();
   libPvr_Term();
}

void plugins_Reset(bool Manual)
{
	libPvr_Reset(Manual);
	libGDR_Reset(Manual);
	libAICA_Reset(Manual);

	arm_Reset();
	arm_SetEnabled(false);

	//libExtDevice_Reset(Manual);
}

#include "rom_luts.h"

static void LoadSpecialSettingsCPU(void)
{
#if FEAT_SHREC != DYNAREC_NONE
	if(settings.dynarec.Enable)
	{
		Get_Sh4Recompiler(&sh4_cpu);
		log_cb(RETRO_LOG_INFO, "Using Recompiler\n");
	}
	else
#endif
	{
		Get_Sh4Interpreter(&sh4_cpu);
		log_cb(RETRO_LOG_INFO, "Using Interpreter\n");
	}
   sh4_cpu.Reset(false);
}

extern bool update_zmax;

static void LoadSpecialSettings(void)
{
   unsigned i;

   log_cb(RETRO_LOG_INFO, "[LUT]: Product number: %s.\n", reios_product_number);
   for (i = 0; i < sizeof(lut_games)/sizeof(lut_games[0]); i++)
   {
      if (strstr(lut_games[i].product_number, reios_product_number))
      {
         log_cb(RETRO_LOG_INFO, "[LUT]: Found game in LUT database..\n");

         if (lut_games[i].dynarec_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying dynarec type hack.\n");
            settings.dynarec.Type = lut_games[i].dynarec_type;
            LoadSpecialSettingsCPU();
         }

         if (lut_games[i].alpha_sort_mode != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying alpha sort hack.\n");
            settings.pvr.Emulation.AlphaSortMode = lut_games[i].alpha_sort_mode;
         }

         if (lut_games[i].updatemode_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying update mode type hack.\n");
            settings.UpdateModeForced = 1;
         }

         if (lut_games[i].translucentPolygonDepthMask != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying translucent polygon depth mask hack.\n");
            settings.rend.TranslucentPolygonDepthMask = lut_games[i].translucentPolygonDepthMask;
         }

         if (lut_games[i].rendertotexturebuffer != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying rendertotexture hack.\n");
            settings.rend.RenderToTextureBuffer = lut_games[i].rendertotexturebuffer;
         }

         if (lut_games[i].disable_div != -1 &&
               settings.dynarec.AutoDivMatching)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying Disable DIV hack.\n");
            settings.dynarec.DisableDivMatching = lut_games[i].disable_div;
         }
         if (lut_games[i].extra_depth_scale != 1 && settings.rend.AutoExtraDepthScale)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying auto extra depth scale.\n");
        	settings.rend.ExtraDepthScale = lut_games[i].extra_depth_scale;
         }

         break;
      }
   }
}

static void LoadSpecialSettingsNaomi(const char *name)
{
   unsigned i;

   log_cb(RETRO_LOG_INFO, "[LUT]: Naomi ROM name is: %s.\n", name);
   for (i = 0; i < sizeof(lut_games_naomi)/sizeof(lut_games_naomi[0]); i++)
   {
      if (strstr(lut_games_naomi[i].product_number, name))
      {
         log_cb(RETRO_LOG_INFO, "[LUT]: Found game in LUT database..\n");

         if (lut_games_naomi[i].dynarec_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying dynarec type hack.\n");
            settings.dynarec.Type = lut_games_naomi[i].dynarec_type;
            LoadSpecialSettingsCPU();
         }

         if (lut_games_naomi[i].alpha_sort_mode != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying alpha sort hack.\n");
            settings.pvr.Emulation.AlphaSortMode = lut_games_naomi[i].alpha_sort_mode;
         }

         if (lut_games_naomi[i].updatemode_type != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying update mode type hack.\n");
            settings.UpdateModeForced = 1;
         }

         if (lut_games_naomi[i].translucentPolygonDepthMask != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying translucent polygon depth mask hack.\n");
            settings.rend.TranslucentPolygonDepthMask = lut_games_naomi[i].translucentPolygonDepthMask;
         }

         if (lut_games_naomi[i].rendertotexturebuffer != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying rendertotexture hack.\n");
            settings.rend.RenderToTextureBuffer = lut_games_naomi[i].rendertotexturebuffer;
         }

         if (lut_games_naomi[i].disable_div != -1 &&
               settings.dynarec.AutoDivMatching)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying Disable DIV hack.\n");
            settings.dynarec.DisableDivMatching = lut_games_naomi[i].disable_div;
         }

         if (lut_games_naomi[i].jamma_setup != -1)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying alternate Jamma I/O board setup.\n");
            settings.mapping.JammaSetup = lut_games_naomi[i].jamma_setup;
         }

         if (lut_games_naomi[i].extra_depth_scale != 1 && settings.rend.AutoExtraDepthScale)
         {
            log_cb(RETRO_LOG_INFO, "[Hack]: Applying auto extra depth scale.\n");
            settings.rend.ExtraDepthScale = lut_games_naomi[i].extra_depth_scale;
         }

         if (lut_games_naomi[i].game_inputs != NULL)
         {
            log_cb(RETRO_LOG_INFO, "Setting custom input descriptors\n");
        	naomi_game_inputs = lut_games_naomi[i].game_inputs;
         }

         break;
      }
   }
}

void dc_prepare_system(void)
{
   BBSRAM_SIZE             = (32*1024);

   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
         //DC : 16 mb ram, 8 mb vram, 2 mb aram, 2 mb bios, 128k flash
         FLASH_SIZE        = (128*1024);
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (16*1024*1024);
         ARAM_SIZE         = (2*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         sys_nvmem_flash.Allocate(FLASH_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_DEV_UNIT:
         //Devkit : 32 mb ram, 8? mb vram, 2? mb aram, 2? mb bios, ? flash
         FLASH_SIZE        = (128*1024);
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (32*1024*1024);
         ARAM_SIZE         = (2*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         sys_nvmem_flash.Allocate(FLASH_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_NAOMI:
         //Naomi : 32 mb ram, 16 mb vram, 8 mb aram, 2 mb bios, ? flash
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (32*1024*1024);
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         sys_nvmem_sram.Allocate(BBSRAM_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_NAOMI2:
         //Naomi2 : 32 mb ram, 16 mb vram, 8 mb aram, 2 mb bios, ? flash
         BIOS_SIZE         = (2*1024*1024);
         RAM_SIZE          = (32*1024*1024);
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (16*1024*1024);
         sys_nvmem_sram.Allocate(BBSRAM_SIZE);
         sys_rom.Allocate(BIOS_SIZE);
         break;
      case DC_PLATFORM_ATOMISWAVE:
         //Atomiswave : 16 mb ram, 8 mb vram, 8 mb aram, 128kb bios+flash, 128k BBSRAM
         FLASH_SIZE        = 0;
         BIOS_SIZE         = (128*1024);
         RAM_SIZE          = (16*1024*1024);
         ARAM_SIZE         = (8*1024*1024);
         VRAM_SIZE         = (8*1024*1024);
         BBSRAM_SIZE       = (128*1024);
         sys_nvmem_flash.Allocate(BIOS_SIZE);
         sys_nvmem_flash.write_protect_size = BIOS_SIZE / 2;
         sys_nvmem_sram.Allocate(BBSRAM_SIZE);
         break;
   }

   RAM_MASK         = (RAM_SIZE-1);
   ARAM_MASK        = (ARAM_SIZE-1);
   VRAM_MASK        = (VRAM_SIZE-1);
}

void dc_reset()
{
	plugins_Reset(false);
	mem_Reset(false);

	sh4_cpu.Reset(false);
}

int dc_init(int argc,wchar* argv[])
{
   char name[128];

   name[0] = '\0';

	setbuf(stdin,0);
	setbuf(stdout,0);
	setbuf(stderr,0);
   extern void common_libretro_setup(void);
   common_libretro_setup();

	if (!_vmem_reserve())
	{
		log_cb(RETRO_LOG_INFO, "Failed to alloc mem\n");
		return -1;
	}

   LoadSettings();
	os_CreateWindow();

	int rv= 0;

   extern char game_dir_no_slash[1024];

   char new_system_dir[1024];

#ifdef _WIN32
   sprintf(new_system_dir, "%s\\", game_dir_no_slash);
#else
   sprintf(new_system_dir, "%s/", game_dir_no_slash);
#endif

   if (
         settings.bios.UseReios || !LoadRomFiles(new_system_dir)
      )
	{
      if (!LoadHle(new_system_dir))
			return -3;
      log_cb(RETRO_LOG_WARN, "Did not load bios, using reios\n");
	}

   LoadSpecialSettingsCPU();

	sh4_cpu.Init();
	mem_Init();

	if (plugins_Init(name, sizeof(name)))
	   return -4;
	
	mem_map_default();

	mcfg_CreateDevices();

	dc_reset();

   const char* bootfile = reios_locate_ip();
   if (!bootfile || !reios_locate_bootfile("1ST_READ.BIN"))
      log_cb(RETRO_LOG_ERROR, "Failed to locate bootfile.\n");

   switch (settings.System)
   {
      case DC_PLATFORM_DREAMCAST:
         LoadSpecialSettings();
         break;
      case DC_PLATFORM_ATOMISWAVE:
      case DC_PLATFORM_NAOMI:
         LoadSpecialSettingsNaomi(naomi_game_id);
         break;
   }

	return rv;
}

void dc_run(void)
{
   sh4_cpu.Run();
}

void dc_term(void)
{
	sh4_cpu.Term();
	plugins_Term();
	_vmem_release();

	SaveRomFiles(get_writable_data_path(""));
}

void dc_stop()
{
	sh4_cpu.Stop();
}


void dc_start()
{
	sh4_cpu.Start();
}

bool dc_is_running()
{
	return sh4_cpu.IsCpuRunning();
}

// Called on the emulator thread for soft reset
void dc_request_reset()
{
	reset_requested = true;
	dc_stop();
}

void LoadSettings(void)
{
   settings.dynarec.Enable			= 1;
	settings.dynarec.idleskip		= 1;
	settings.dynarec.unstable_opt	= 0; 
   //settings.dynarec.DisableDivMatching       = 0;
	//disable_nvmem can't be loaded, because nvmem init is before cfg load
   settings.UpdateModeForced     = 0;
	settings.dreamcast.RTC			= GetRTC_now();
	settings.aica.LimitFPS			= 0;
   settings.aica.NoSound			= 0;
	settings.pvr.subdivide_transp	= 0;
	settings.pvr.ta_skip			   = 0;
   //settings.pvr.Emulation.AlphaSortMode= 0;
   settings.pvr.Emulation.zMin         = 0.f;
   settings.pvr.Emulation.zMax         = 1.0f;

#if !defined(NO_MMU)
   settings.MMUEnabled                  = true;
#endif
	settings.pvr.MaxThreads			       = 3;
#ifndef __LIBRETRO__
   settings.pvr.Emulation.ModVol       = true;
   settings.rend.RenderToTextureBuffer  = false;
   settings.rend.RenderToTexture        = true;
   settings.rend.RenderToTextureUpscale = 1;
   settings.rend.MaxFilteredTextureSize = 256;
   settings.pvr.SynchronousRendering	 = 0;
#endif
   settings.rend.AutoExtraDepthScale    = true;
   settings.rend.ExtraDepthScale        = 1.f;

   settings.rend.Clipping               = true;


   settings.rend.ModifierVolumes        = true;
   settings.rend.TranslucentPolygonDepthMask = false;

	settings.debug.SerialConsole         = 0;

	settings.reios.ElfFile               = "";

	settings.validate.OpenGlChecks      = 0;

	settings.bios.UseReios              = 0;
}

void SaveSettings(void)
{
}

