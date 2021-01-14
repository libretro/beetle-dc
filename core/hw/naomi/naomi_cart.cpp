/*
	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
*/
// Naomi comm board emulation from mame
// https://github.com/mamedev/mame/blob/master/src/mame/machine/m3comm.cpp
// license:BSD-3-Clause
// copyright-holders:MetalliC

#include <memory>
#include "naomi_cart.h"
#include "naomi_regs.h"
#include "naomi.h"
#include "decrypt.h"
#include "naomi_roms.h"
#include "hw/flashrom/flashrom.h"
#include "hw/holly/holly_intc.h"
#include "m1cartridge.h"
#include "m4cartridge.h"
#include "awcartridge.h"
#include "gdcartridge.h"
#include "archive/archive.h"
#include "file/file_path.h"

Cartridge *CurrentCartridge;
bool bios_loaded = false;

#ifdef _WIN32
#include <windows.h>
typedef HANDLE fd_t;
#define INVALID_FD INVALID_HANDLE_VALUE
#ifndef FILE_READ_ACCESS
#define FILE_READ_ACCESS        0x0001
#endif

#ifndef FILE_WRITE_ACCESS
#define FILE_WRITE_ACCESS       0x0002
#endif
#else
typedef int fd_t;
#define INVALID_FD -1

#include <fcntl.h>
#ifndef VITA
#include <sys/mman.h>
#endif
#include <errno.h>
#endif
#include <unistd.h>

fd_t *RomCacheMap;
u32 RomCacheMapCount;

char naomi_game_id[33];
char g_parent_name[128];

extern RomChip sys_rom;
extern DCFlashChip sys_nvmem_flash;		// AtomisWave BIOS is loaded there

extern char game_dir_no_slash[1024];
extern char g_roms_dir[PATH_MAX];

InputDescriptors *naomi_game_inputs;
u8 *naomi_default_eeprom;
static RotationType game_rotation = ROT0;

static bool naomi_LoadBios(const char *filename, Archive *child_archive, Archive *parent_archive, int region)
{
	int biosid = 0;
	for (; BIOS[biosid].name != NULL; biosid++)
		if (!stricmp(BIOS[biosid].name, filename))
			break;
	if (BIOS[biosid].name == NULL)
	{
		WARN_LOG(NAOMI, "Unknown BIOS %s", filename);
		return false;
	}

	MemChip *rom_chip;
	if (settings.System == DC_PLATFORM_NAOMI)
	   rom_chip = &sys_rom;
	else
	   rom_chip = &sys_nvmem_flash;

	struct BIOS_t *bios = &BIOS[biosid];

	std::string basepath(game_dir_no_slash);
	basepath += path_default_slash();

	Archive *bios_archive = OpenArchive((basepath + filename).c_str());

	bool found_region = false;

	for (int romid = 0; bios->blobs[romid].filename != NULL; romid++)
	{
	   if (region == -1)
		  region = bios->blobs[romid].region;
	   else
	   {
		  if (bios->blobs[romid].region != region)
			 continue;
	   }
	   found_region = true;

	    if (bios->blobs[romid].blob_type == Copy)
		{
			verify(bios->blobs[romid].offset + bios->blobs[romid].length <= rom_chip->size);
			verify(bios->blobs[romid].src_offset + bios->blobs[romid].length <= rom_chip->size);
			memcpy(rom_chip->data + bios->blobs[romid].offset, rom_chip->data + bios->blobs[romid].src_offset, bios->blobs[romid].length);
		}
		else
		{
			ArchiveFile *file = NULL;
			if (child_archive != NULL)
			   file = child_archive->OpenFileByCrc(bios->blobs[romid].crc);
			if (file == NULL && parent_archive != NULL)
				file = parent_archive->OpenFileByCrc(bios->blobs[romid].crc);
			if (file == NULL && bios_archive != NULL)
				file = bios_archive->OpenFileByCrc(bios->blobs[romid].crc);
			if (file == NULL && child_archive != NULL)
			   file = child_archive->OpenFile(bios->blobs[romid].filename);
			if (file == NULL && parent_archive != NULL)
				file = parent_archive->OpenFile(bios->blobs[romid].filename);
			if (file == NULL && bios_archive != NULL)
				file = bios_archive->OpenFile(bios->blobs[romid].filename);
			if (!file) {
				WARN_LOG(NAOMI, "%s: Cannot open %s", filename, bios->blobs[romid].filename);
				goto error;
			}
			if (bios->blobs[romid].blob_type == Normal)
			{
				verify(bios->blobs[romid].offset + bios->blobs[romid].length <= rom_chip->size);
				u32 read = file->Read(rom_chip->data + bios->blobs[romid].offset, bios->blobs[romid].length);
			}
			else if (bios->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(bios->blobs[romid].length);
				if (buf == NULL)
				{
					WARN_LOG(NAOMI, "malloc failed");
					delete file;
					goto error;
				}
				verify(bios->blobs[romid].offset + bios->blobs[romid].length <= rom_chip->size);
				u32 read = file->Read(buf, bios->blobs[romid].length);
				u16 *to = (u16 *)(rom_chip->data + bios->blobs[romid].offset);
				u16 *from = (u16 *)buf;
				for (int i = bios->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
			}
			else
				die("Unknown blob type");
			delete file;
		}
	}

	if (bios_archive != NULL)
		delete bios_archive;

	if (settings.System == DC_PLATFORM_ATOMISWAVE)
	   // Reload the writeable portion of the FlashROM
	   sys_nvmem_flash.Reload();

	return found_region;

error:
	if (bios_archive != NULL)
		delete bios_archive;
	return false;
}

static bool naomi_cart_LoadZip(const char *filename)
{
	char game_name[128];
	strncpy(game_name, path_basename(filename), sizeof(game_name) - 1);
	game_name[sizeof(game_name) - 1] = '\0';
	path_remove_extension(game_name);

	int gameid = 0;
	for (; Games[gameid].name != NULL; gameid++)
		if (!stricmp(Games[gameid].name, game_name))
			break;
	if (Games[gameid].name == NULL)
	{
		WARN_LOG(NAOMI, "Unknown game %s", game_name);
		return false;
	}

	struct Game *game = &Games[gameid];

	Archive *archive = OpenArchive(filename);
	if (archive != NULL)
		INFO_LOG(NAOMI, "Opened %s", filename);

	Archive *parent_archive = NULL;
	if (game->parent_name != NULL)
	{
	   strncpy(g_parent_name, game->parent_name, sizeof(g_parent_name));
	   std::string parent_path(g_roms_dir);
	   parent_path += path_default_slash();
	   parent_path += game->parent_name;
	   parent_archive = OpenArchive(parent_path.c_str());
	   if (parent_archive != NULL)
		  INFO_LOG(NAOMI, "Opened %s", game->parent_name);
	}

	if (archive == NULL && parent_archive == NULL)
	{
		if (game->parent_name != NULL)
			WARN_LOG(NAOMI, "Cannot open %s or %s", filename, game->parent_name);
		else
			WARN_LOG(NAOMI, "Cannot open %s", filename);
		return false;
	}

	const char *bios = "naomi";
	if (game->bios != NULL)
	   bios = game->bios;
	u32 region_flag = settings.dreamcast.region;
	if (region_flag > game->region_flag)
	   region_flag = game->region_flag;
	if (game->region_flag == REGION_EXPORT_ONLY)
	   region_flag = REGION_EXPORT;
	if (!naomi_LoadBios(bios, archive, parent_archive, region_flag))
	{
	   WARN_LOG(NAOMI, "Warning: Region %d bios not found in %s", settings.dreamcast.region, bios);
	   if (!naomi_LoadBios(bios, archive, parent_archive, -1))
	   {
		  // If a specific BIOS is needed for this game, fail.
		  if (game->bios != NULL || !bios_loaded)
		  {
			 ERROR_LOG(NAOMI, "Error: cannot load BIOS. Exiting");
			 return false;
		  }
		  // otherwise use the default BIOS
	   }
	}
	bios_loaded = true;

	switch (game->cart_type)
	{
	case M1:
		CurrentCartridge = new M1Cartridge(game->size);
		break;
	case M2:
		CurrentCartridge = new M2Cartridge(game->size);
		break;
	case M4:
		CurrentCartridge = new M4Cartridge(game->size);
		break;
	case AW:
		CurrentCartridge = new AWCartridge(game->size);
		break;
	case GD:
	    {
	       GDCartridge *gdcart = new GDCartridge(game->size);
	       gdcart->SetGDRomName(game->gdrom_name);
	       CurrentCartridge = gdcart;
	    }
	    break;
	default:
		die("Unsupported cartridge type");
		break;
	}
	CurrentCartridge->SetKey(game->key);
	naomi_game_inputs = game->inputs;

	for (int romid = 0; game->blobs[romid].filename != NULL; romid++)
	{
		u32 len = game->blobs[romid].length;

		if (game->blobs[romid].blob_type == Copy)
		{
			u8 *dst = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
			u8 *src = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].src_offset, len);
			memcpy(dst, src, game->blobs[romid].length);
			DEBUG_LOG(NAOMI, "Copied: %x bytes from %07x to %07x", game->blobs[romid].length, game->blobs[romid].src_offset, game->blobs[romid].offset);
		}
		else
		{
			ArchiveFile* file = NULL;
			if (archive != NULL)
				file = archive->OpenFileByCrc(game->blobs[romid].crc);
			if (file == NULL && parent_archive != NULL)
				file = parent_archive->OpenFileByCrc(game->blobs[romid].crc);
			if (file == NULL && archive != NULL)
				file = archive->OpenFile(game->blobs[romid].filename);
			if (file == NULL && parent_archive != NULL)
				file = parent_archive->OpenFile(game->blobs[romid].filename);
			if (!file) {
				WARN_LOG(NAOMI, "%s: Cannot open %s", filename, game->blobs[romid].filename);
				if (game->blobs[romid].blob_type != Eeprom)
				   // Default eeprom file is optional
				   goto error;
				else
				   continue;
			}
			if (game->blobs[romid].blob_type == Normal)
			{
				u8 *dst = (u8 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
				u32 read = file->Read(dst, game->blobs[romid].length);
				DEBUG_LOG(NAOMI, "Mapped %s: %x bytes at %07x", game->blobs[romid].filename, read, game->blobs[romid].offset);
			}
			else if (game->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					ERROR_LOG(NAOMI, "malloc failed");
					delete file;
					goto error;
				}
				u32 read = file->Read(buf, game->blobs[romid].length);
				u16 *to = (u16 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
				u16 *from = (u16 *)buf;
				for (int i = game->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
				DEBUG_LOG(NAOMI, "Mapped %s: %x bytes (interleaved word) at %07x", game->blobs[romid].filename, read, game->blobs[romid].offset);
			}
			else if (game->blobs[romid].blob_type == Key)
			{
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					ERROR_LOG(NAOMI, "malloc failed");
					delete file;
					goto error;
				}
				u32 read = file->Read(buf, game->blobs[romid].length);
				CurrentCartridge->SetKeyData(buf);
				DEBUG_LOG(NAOMI, "Loaded %s: %x bytes cart key", game->blobs[romid].filename, read);
			}
			else if (game->blobs[romid].blob_type == Eeprom)
			{
			    naomi_default_eeprom = (u8 *)malloc(game->blobs[romid].length);
			    if (naomi_default_eeprom == NULL)
			    {
					ERROR_LOG(NAOMI, "malloc failed");
					delete file;
			       goto error;
			    }
				u32 read = file->Read(naomi_default_eeprom, game->blobs[romid].length);
				DEBUG_LOG(NAOMI, "Loaded %s: %x bytes default eeprom", game->blobs[romid].filename, read);
			}
			else
				die("Unknown blob type");
			delete file;
		}
	}
	if (naomi_default_eeprom == NULL && game->eeprom_dump != NULL)
		naomi_default_eeprom = game->eeprom_dump;
	game_rotation = game->rotation_flag;
	if (archive != NULL)
		delete archive;
	if (parent_archive != NULL)
		delete parent_archive;

	CurrentCartridge->Init();

	strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
	if (naomi_game_id[0] == '\0')
		strcpy(naomi_game_id, game->name);
	NOTICE_LOG(NAOMI, "NAOMI GAME ID [%s]", naomi_game_id);

	return true;

error:
	if (archive != NULL)
		delete archive;
	if (parent_archive != NULL)
		delete parent_archive;
	delete CurrentCartridge;
	CurrentCartridge = NULL;
	return false;
}

int naomi_cart_GetSystemType(const char* file)
{
	const char *ext = path_get_extension(file);

   if (strcasecmp(ext, "zip") && strcasecmp(ext, "7z"))
	  // Not a ZIP or 7z file so it has to be a Naomi game
	  return DC_PLATFORM_NAOMI;

	char game_name[128];
	strncpy(game_name, path_basename(file), sizeof(game_name) - 1);
	game_name[sizeof(game_name) - 1] = '\0';
	path_remove_extension(game_name);

   int gameid = 0;
   for (; Games[gameid].name != NULL; gameid++)
	  if (!stricmp(Games[gameid].name, game_name))
		 break;
   if (Games[gameid].name == NULL)
	  // Not found. Will fail later
	  return DC_PLATFORM_NAOMI;

   if (Games[gameid].cart_type == AW)
	  return DC_PLATFORM_ATOMISWAVE;
   else
	  return DC_PLATFORM_NAOMI;
}

int naomi_cart_GetRotation()
{
	return game_rotation;
}

#ifdef _WIN32
#define CloseFile(f)	CloseHandle(f)
#else
#define CloseFile(f)	close(f)
#endif

void naomi_cart_Close()
{
	if (CurrentCartridge != NULL)
	{
		delete CurrentCartridge;
		CurrentCartridge = NULL;
	}
	if (RomCacheMap != NULL)
	{
		for (int i = 0; i < RomCacheMapCount; i++)
			if (RomCacheMap[i] != INVALID_FD)
				CloseFile(RomCacheMap[i]);
		RomCacheMapCount = 0;
		delete[] RomCacheMap;
		RomCacheMap = NULL;
	}
	bios_loaded = false;
}

static bool naomi_cart_LoadRom(const char* file)
{
	INFO_LOG(NAOMI, "nullDC-Naomi rom loader v1.2");

	naomi_cart_Close();

	size_t folder_pos = strlen(file) - 1;
	while (folder_pos>1 && (file[folder_pos] != '\\' && file[folder_pos] != '/'))
		folder_pos--;

	folder_pos++;

	vector<string> files;
	vector<u32> fstart;
	vector<u32> fsize;

	u32 setsize = 0;
	bool raw_bin_file = false;

	char t[512];
	strcpy(t, file);

	const char *ext = path_get_extension(file);

	if (!strcasecmp(ext, "zip")	|| !strcasecmp(ext, "7z"))
		return naomi_cart_LoadZip(file);

	// Try to load BIOS from naomi.zip
	if (!naomi_LoadBios("naomi", NULL, NULL, settings.dreamcast.region))
	{
		WARN_LOG(NAOMI, "Warning: Region %d bios not found in naomi.zip", settings.dreamcast.region);
	   if (!naomi_LoadBios("naomi", NULL, NULL, -1))
	   {
		  if (!bios_loaded)
		  {
			 ERROR_LOG(NAOMI, "Error: cannot load BIOS. Exiting");
			 return false;
		  }
	   }
	}

	u8* RomPtr;
	u32 RomSize;

	if (!strcasecmp(ext, "lst"))
	{
	   FILE* fl = fopen(t, "r");
	   if (!fl)
		   return false;

	   char* line = fgets(t, 512, fl);
	   if (!line)
	   {
		   fclose(fl);
		   return false;
	   }

	   char* eon = strstr(line, "\n");
	   if (!eon)
	   {
		   ERROR_LOG(NAOMI, "+Parsing was unsuccessful, there is something wrong with your lst file");
		   fclose(fl);
		   return false;
	   }
	   else
	   {
		   *eon = 0;
	   }
	   eon = strstr(line, "\r");
	   if (eon)
		   *eon = 0;

	   DEBUG_LOG(NAOMI, "+Loading naomi rom : %s", line);

	   line = fgets(t, 512, fl);
	   if (!line)
	   {
		   fclose(fl);
		   return false;
	   }

	   RomSize = 0;

	   while (line)
	   {
		   char filename[512];
		   u32 addr, sz;
		   if (sscanf(line, "\"%[^\"]\",%x,%x", filename, &addr, &sz) == 3)
		   {
			  files.push_back(filename);
			  fstart.push_back(addr);
			  fsize.push_back(sz);
			  setsize += sz;
			  RomSize = max(RomSize, (addr + sz));
		   }
		   else if (line[0] != 0 && line[0] != '\n' && line[0] != '\r')
				WARN_LOG(NAOMI, "Warning: invalid line in .lst file: %s", line);

		   line = fgets(t, 512, fl);
	   }
	   fclose(fl);
	}
	else
	{
	   // BIN loading
	   FILE* fp = fopen(t, "rb");
	   if (fp == NULL)
		  return false;
	   fseek(fp, 0, SEEK_END);
	   u32 file_size = ftell(fp);
	   fclose(fp);
	   files.push_back(t);
	   fstart.push_back(0);
	   fsize.push_back(file_size);
	   setsize = file_size;
	   RomSize = file_size;
	   raw_bin_file = true;
	}

	INFO_LOG(NAOMI, "+%zd romfiles, %.2f MB set size, %.2f MB set address space", files.size(), setsize / 1024.f / 1024.f, RomSize / 1024.f / 1024.f);

	if (RomCacheMap)
	{
		for (int i = 0; i < RomCacheMapCount; i++)
			if (RomCacheMap[i] != INVALID_FD)
				CloseFile(RomCacheMap[i]);
		RomCacheMapCount = 0;
		delete[] RomCacheMap;
	}

	RomCacheMapCount = (u32)files.size();
	RomCacheMap = new fd_t[files.size()]();

	//Allocate space for the ram, so we are sure we have a segment of continuous ram
	RomPtr = (u8*)mem_region_reserve(NULL, RomSize);
	verify(RomPtr != NULL);
	strcpy(t, file);

	bool load_error = false;

	//Create File Mapping Objects
	for (size_t i = 0; i<files.size(); i++)
	{
		if (!raw_bin_file)
		{
		   strncpy(t, file, sizeof(t));
		   t[sizeof(t) - 1] = '\0';
		   t[folder_pos] = 0;
		   strcat(t, files[i].c_str());
		}
		else
		{
		   strncpy(t, files[i].c_str(), sizeof(t));
		   t[sizeof(t) - 1] = '\0';
		}
		fd_t RomCache;

		if (strcmp(files[i].c_str(), "null") == 0)
		{
			RomCacheMap[i] = INVALID_FD;
			continue;
		}
#ifdef _WIN32
		RomCache = CreateFile(t, FILE_READ_ACCESS, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
#else
		RomCache = open(t, O_RDONLY);
#endif
		if (RomCache == INVALID_FD)
		{
		   ERROR_LOG(NAOMI, "-Unable to read file %s: error %d", t, errno);
		   RomCacheMap[i] = INVALID_FD;
		   load_error = true;
		   break;
		}

#ifdef _WIN32
		RomCacheMap[i] = CreateFileMapping(RomCache, 0, PAGE_READONLY, 0, fsize[i], 0);
		verify(CloseHandle(RomCache));
#else
		RomCacheMap[i] = RomCache;
#endif

		verify(RomCacheMap[i] != INVALID_FD);
		//printf("-Preparing \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
	}

	//Release the segment we reserved so we can map the files there
	mem_region_release(RomPtr, RomSize);

	if (load_error)
	{
	   for (size_t i = 0; i < files.size(); i++)
		  if (RomCacheMap[i] != INVALID_FD)
			 close(RomCacheMap[i]);
	   return false;
	}
	//We have all file mapping objects, we start to map the ram

	//Map the files into the segment of the ram that was reserved
	for (size_t i = 0; i<RomCacheMapCount; i++)
	{
		u8* RomDest = RomPtr + fstart[i];

		if (RomCacheMap[i] == INVALID_FD)
		{
			//printf("-Reserving ram at 0x%08X, size 0x%08X\n", fstart[i], fsize[i]);
			
			bool mapped = RomDest == (u8 *)mem_region_reserve(RomDest, fsize[i]);
			if (!mapped)
			{
			   ERROR_LOG(NAOMI, "-Mapping RAM FAILED @ %08x size %x", fstart[i], fsize[i]);
			   return false;
			}
		}
		else
		{
			//printf("-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
			bool mapped = RomDest == (u8 *)mem_region_map_file((void *)(uintptr_t)RomCacheMap[i], RomDest, fsize[i], 0, false);
			if (!mapped)
			{
			   ERROR_LOG(NAOMI, "-Mapping ROM FAILED: %s @ %08x size %x", files[i].c_str(), fstart[i], fsize[i]);
			   return false;
			}
		}
	}

	//done :)
	INFO_LOG(NAOMI, "Mapped ROM Successfully !");

	CurrentCartridge = new DecryptedCartridge(RomPtr, RomSize);
	strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
	NOTICE_LOG(NAOMI, "NAOMI GAME ID [%s]", naomi_game_id);

	return true;
}

extern char *game_data;
extern char eeprom_file[PATH_MAX];

bool naomi_cart_SelectFile()
{
	if (!naomi_cart_LoadRom(game_data))
	{
	   ERROR_LOG(NAOMI, "Cannot load %s: error %d", game_data, errno);
	   return false;
	}

	INFO_LOG(NAOMI, "EEPROM file : %s", eeprom_file);

	return true;
}

Cartridge::Cartridge(u32 size)
{
	RomPtr = (u8 *)malloc(size);
	RomSize = size;
	memset(RomPtr, 0xFF, RomSize);
}

Cartridge::~Cartridge()
{
	if (RomPtr != NULL)
		free(RomPtr);
}

bool Cartridge::Read(u32 offset, u32 size, void* dst)
{
	offset &= 0x1FFFFFFF;
	if (offset >= RomSize || (offset + size) > RomSize)
	{
		static u32 ones = 0xffffffff;

		// Makes Outtrigger boot
		INFO_LOG(NAOMI, "offset %d > %d", offset, RomSize);
		memcpy(dst, &ones, size);
	}
	else
	{
		memcpy(dst, &RomPtr[offset], size);
	}

	return true;
}

bool Cartridge::Write(u32 offset, u32 size, u32 data)
{
	INFO_LOG(NAOMI, "Invalid write @ %08x data %x", offset, data);
	return false;
}

void* Cartridge::GetPtr(u32 offset, u32& size)
{
	offset &= 0x1FFFffff;

	verify(offset < RomSize);
	verify((offset + size) <= RomSize);

	return &RomPtr[offset];
}

std::string Cartridge::GetGameId() {
	if (RomSize < 0x30 + 0x20)
		return "(ROM too small)";

	std::string game_id((char *)RomPtr + 0x30, 0x20);
	if (game_id == "AWNAOMI                         " && RomSize >= 0xFF50)
	{
		game_id = std::string((char *)RomPtr + 0xFF30, 0x20);
	}
	while (!game_id.empty() && game_id.back() == ' ')
		game_id.pop_back();
	return game_id;
}

void* NaomiCartridge::GetDmaPtr(u32& size)
{
	if ((DmaOffset & 0x1fffffff) >= RomSize)
	{
		INFO_LOG(NAOMI, "Error: DmaOffset >= RomSize");
		size = 0;
		return NULL;
	}
	size = min(size, RomSize - (DmaOffset & 0x1fffffff));
	return GetPtr(DmaOffset, size);
}

void NaomiCartridge::AdvancePtr(u32 size) {
}

u32 NaomiCartridge::ReadMem(u32 address, u32 size)
{
	verify(size!=1);
	//printf("+naomi?WTF? ReadMem: %X, %d\n", address, size);
	switch(address & 255)
	{
	case 0x3c:	// 5f703c: DIMM COMMAND
		DEBUG_LOG(NAOMI, "DIMM COMMAND read<%d>", size);
		return 0xffff; // reg_dimm_command;
	case 0x40:	// 5f7040: DIMM OFFSETL
		DEBUG_LOG(NAOMI, "DIMM OFFSETL read<%d>", size);
		return reg_dimm_offsetl;
	case 0x44:	// 5f7044: DIMM PARAMETERL
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL read<%d>", size);
		return reg_dimm_parameterl;
	case 0x48:	// 5f7048: DIMM PARAMETERH
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH read<%d>", size);
		return reg_dimm_parameterh;
	case 0x04C:	// 5f704c: DIMM STATUS
		DEBUG_LOG(NAOMI, "DIMM STATUS read<%d>", size);
		return reg_dimm_status;

	case NAOMI_ROM_OFFSETH_addr&255:
		return RomPioOffset>>16 | (RomPioAutoIncrement << 15);

	case NAOMI_ROM_OFFSETL_addr&255:
		return RomPioOffset&0xFFFF;

	case NAOMI_ROM_DATA_addr & 255:
		{
			u32 rv = 0;
			Read(RomPioOffset, 2, &rv);
			if (RomPioAutoIncrement)
				RomPioOffset += 2;

			return rv;
		}

	case NAOMI_DMA_COUNT_addr&255:
		return (u16) DmaCount;

	case NAOMI_BOARDID_READ_addr&255:
		return NaomiGameIDRead()?0x8000:0x0000;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
		#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM offs READ: %X, %d", address, size);
		return CommOffset;
		#endif
	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM data read: %X, %d", CommOffset, size);
		if (CommSharedMem)
		{
			return CommSharedMem[CommOffset&0xF];
		}
		#endif
		return 1;


	case NAOMI_DMA_OFFSETH_addr&255:
		return DmaOffset>>16;
	case NAOMI_DMA_OFFSETL_addr&255:
		return DmaOffset&0xFFFF;

	case NAOMI_BOARDID_WRITE_addr&255:
		DEBUG_LOG(NAOMI, "naomi ReadBoardId: %X, %d", address, size);
		return 1;

	case NAOMI_COMM2_CTRL_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL read");
		return comm_ctrl;

	case NAOMI_COMM2_OFFSET_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_OFFSET read");
		return comm_offset;

	case NAOMI_COMM2_DATA_addr & 255:
		{
			DEBUG_LOG(NAOMI, "NAOMI_COMM2_DATA read @ %04x", comm_offset);
			u16 value;
			if (comm_ctrl & 1)
				value = m68k_ram[comm_offset / 2];
			else {
				// TODO u16 *commram = (u16*)membank("comm_ram")->base();
				value = comm_ram[comm_offset / 2];
			}
			comm_offset += 2;
			return value;
		}

	case NAOMI_COMM2_STATUS0_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS0 read");
		return comm_status0;

	case NAOMI_COMM2_STATUS1_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS1 read");
		return comm_status1;

	default: break;
	}
	DEBUG_LOG(NAOMI, "naomi?WTF? ReadMem: %X, %d", address, size);

	return 0xFFFF;
}

void NaomiCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	switch(address & 255)
	{
	case 0x3c:	// 5f703c: DIMM COMMAND
		 if (0x1E03==data)
		 {
			 /*
			 if (!(reg_dimm_status & 0x100))
				asic_RaiseInterrupt(holly_EXP_PCI);
			 reg_dimm_status |= 1;*/
		 }
		 reg_dimm_command = data;
		 DEBUG_LOG(NAOMI, "DIMM COMMAND Write: %X <= %X, %d", address, data, size);
		 return;

	case 0x40:	// 5f7040: DIMM OFFSETL
		reg_dimm_offsetl = data;
		DEBUG_LOG(NAOMI, "DIMM OFFSETL Write: %X <= %X, %d", address, data, size);
		return;
	case 0x44:	// 5f7044: DIMM PARAMETERL
		reg_dimm_parameterl = data;
		DEBUG_LOG(NAOMI, "DIMM PARAMETERL Write: %X <= %X, %d", address, data, size);
		return;
	case 0x48:	// 5f7048: DIMM PARAMETERH
		reg_dimm_parameterh = data;
		DEBUG_LOG(NAOMI, "DIMM PARAMETERH Write: %X <= %X, %d", address, data, size);
		return;

	case 0x4C:	// 5f704c: DIMM STATUS
		if (data&0x100)
		{
			asic_CancelInterrupt(holly_EXP_PCI);
		}
		else if ((data&1)==0)
		{
			/*FILE* ramd=fopen("c:\\ndc.ram.bin","wb");
			fwrite(mem_b.data,1,RAM_SIZE,ramd);
			fclose(ramd);*/
			naomi_process(reg_dimm_command, reg_dimm_offsetl, reg_dimm_parameterl, reg_dimm_parameterh);
		}
		reg_dimm_status = data & ~0x100;
		DEBUG_LOG(NAOMI, "DIMM STATUS Write: %X <= %X, %d", address, data, size);
		return;

		//These are known to be valid on normal ROMs and DIMM board
	case NAOMI_ROM_OFFSETH_addr&255:
		RomPioAutoIncrement = (data & 0x8000) != 0;
		RomPioOffset&=0x0000ffff;
		RomPioOffset|=(data<<16)&0x7fff0000;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_OFFSETL_addr&255:
		RomPioOffset&=0xffff0000;
		RomPioOffset|=data;
		PioOffsetChanged(RomPioOffset);
		return;

	case NAOMI_ROM_DATA_addr&255:
		Write(RomPioOffset, size, data);
		if (RomPioAutoIncrement)
			RomPioOffset += 2;

		return;

	case NAOMI_DMA_OFFSETH_addr&255:
		DmaOffset&=0x0000ffff;
		DmaOffset|=(data&0x7fff)<<16;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_OFFSETL_addr&255:
		DmaOffset&=0xffff0000;
		DmaOffset|=data;
		DmaOffsetChanged(DmaOffset);
		return;

	case NAOMI_DMA_COUNT_addr&255:
		DmaCount=data;
		return;

	case NAOMI_STATUS_LEDS_addr & 255:
		return;

	case NAOMI_BOARDID_WRITE_addr&255:
		NaomiGameIDWrite((u16)data);
		return;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM ofset Write: %X <= %X, %d", address, data, size);
		CommOffset=data&0xFFFF;
#endif
		return;

	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		DEBUG_LOG(NAOMI, "naomi COMM data Write: %X <= %X, %d", CommOffset, data, size);
		if (CommSharedMem)
		{
			CommSharedMem[CommOffset&0xF]=data;
		}
		#endif
		return;

		//This should be valid
	case NAOMI_BOARDID_READ_addr&255:
		DEBUG_LOG(NAOMI, "naomi WriteMem: %X <= %X, %d", address, data, size);
		return;

	case NAOMI_COMM2_CTRL_addr & 255:
		comm_ctrl = (u16)data;
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL set to %x", comm_ctrl);
		return;

	case NAOMI_COMM2_OFFSET_addr & 255:
		comm_offset = (u16)data;
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_OFFSET set to %x", comm_offset);
		return;

	case NAOMI_COMM2_DATA_addr & 255:
		if (comm_ctrl & 1)
			m68k_ram[comm_offset / 2] = (u16)data;
		else {
			// TODO u16 *commram = (u16*)membank("comm_ram")->base();
			comm_ram[comm_offset / 2] = (u16)data;
		}
		comm_offset += 2;
		return;

	case NAOMI_COMM2_STATUS0_addr & 255:
		comm_status0 = (u16)data;
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS0 set to %x", comm_status0);
		return;

	case NAOMI_COMM2_STATUS1_addr & 255:
		comm_status1 = (u16)data;
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS1 set to %x", comm_status1);
		return;

	default:
		break;
	}
	DEBUG_LOG(NAOMI, "naomi?WTF? WriteMem: %X <= %X, %d", address, data, size);
}

void NaomiCartridge::Serialize(void** data, unsigned int* total_size)
{
   LIBRETRO_S(RomPioOffset);
   LIBRETRO_S(RomPioAutoIncrement);
   LIBRETRO_S(DmaOffset);
   LIBRETRO_S(DmaCount);
   Cartridge::Serialize(data, total_size);
}

void NaomiCartridge::Unserialize(void** data, unsigned int* total_size)
{
   LIBRETRO_US(RomPioOffset);
   LIBRETRO_US(RomPioAutoIncrement);
   LIBRETRO_US(DmaOffset);
   LIBRETRO_US(DmaCount);
   Cartridge::Unserialize(data, total_size);
}

bool M2Cartridge::Read(u32 offset, u32 size, void* dst)
{
	if (offset & 0x40000000)
	{
		if (offset == 0x4001fffe)
		{
			//printf("NAOMI CART DECRYPT read: %08x sz %d\n", offset, size);
			cyptoSetKey(key);
			u16 data = cryptoDecrypt();
			*(u16 *)dst = data;
			return true;
		}
		INFO_LOG(NAOMI, "Invalid read @ %08x", offset);
		return false;
	}
	else if (!(RomPioOffset & 0x20000000))
	{
		// 4MB mode
		offset = (offset & 0x103fffff) | ((offset & 0x07c00000) << 1);
	}
	return NaomiCartridge::Read(offset, size, dst);
}

void* M2Cartridge::GetDmaPtr(u32& size)
{
	if (RomPioOffset & 0x20000000)
		return NaomiCartridge::GetDmaPtr(size);

	// 4MB mode
	u32 offset4mb = (DmaOffset & 0x103fffff) | ((DmaOffset & 0x07c00000) << 1);
	size = min(min(size, 0x400000 - (offset4mb & 0x3FFFFF)), RomSize - offset4mb);

	return GetPtr(offset4mb, size);
}

bool M2Cartridge::Write(u32 offset, u32 size, u32 data)
{
	if (offset & 0x40000000)
	{
		//printf("NAOMI CART CRYPT write: %08x data %x sz %d\n", offset, data, size);
		if (offset & 0x00020000)
		{
			offset &= sizeof(naomi_cart_ram) - 1;
			naomi_cart_ram[offset] = data;
			naomi_cart_ram[offset + 1] = data >> 8;
			return true;
		}
		switch (offset & 0x1ffff)
		{
			case 0x1fff8:
				cyptoSetLowAddr(data);
				return true;
			case 0x1fffa:
				cyptoSetHighAddr(data);
				return true;
			case 0x1fffc:
				cyptoSetSubkey(data);
				return true;
		}
	}
	return NaomiCartridge::Write(offset, size, data);
}

u16 M2Cartridge::ReadCipheredData(u32 offset)
{
	if ((offset & 0xffff0000) == 0x01000000)
	{
		int base = 2 * (offset & 0x7fff);
		return naomi_cart_ram[base + 1] | (naomi_cart_ram[base] << 8);
	}
	verify(2 * offset + 1 < RomSize);
	return RomPtr[2 * offset + 1] | (RomPtr[2 * offset] << 8);

}

std::string M2Cartridge::GetGameId()
{
	std::string game_id = NaomiCartridge::GetGameId();
	if ((game_id.size() < 2 || ((u8)game_id[0] == 0xff && (u8)game_id[1] == 0xff)) && RomSize >= 0x800050)
	{
		game_id = std::string((char *)RomPtr + 0x800030, 0x20);
		while (!game_id.empty() && game_id.back() == ' ')
			game_id.pop_back();
	}
	return game_id;
}

void M2Cartridge::Serialize(void** data, unsigned int* total_size) {
   LIBRETRO_S(naomi_cart_ram);
   NaomiCartridge::Serialize(data, total_size);
}

void M2Cartridge::Unserialize(void** data, unsigned int* total_size) {
   LIBRETRO_US(naomi_cart_ram);
   NaomiCartridge::Unserialize(data, total_size);
}

DecryptedCartridge::~DecryptedCartridge()
{
	// TODO this won't work on windows -> need to unmap each file first
	mem_region_release(RomPtr, RomSize);
	// Avoid crash when freeing vmem
	RomPtr = NULL;
}
