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
#include <sys/mman.h>
#include <errno.h>
#endif
#include <unistd.h>

fd_t*	RomCacheMap;
u32		RomCacheMapCount;

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
		printf("Unknown BIOS %s\n", filename);
		return false;
	}

	MemChip *rom_chip;
	if (settings.System == DC_PLATFORM_NAOMI)
	   rom_chip = &sys_rom;
	else
	   rom_chip = &sys_nvmem_flash;

	struct BIOS_t *bios = &BIOS[biosid];

	std::string basepath(game_dir_no_slash);
	basepath += "/";

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
			   file = child_archive->OpenFile(bios->blobs[romid].filename);
			if (file == NULL && parent_archive != NULL)
				file = parent_archive->OpenFile(bios->blobs[romid].filename);
			if (file == NULL && bios_archive != NULL)
				file = bios_archive->OpenFile(bios->blobs[romid].filename);
			if (!file) {
				printf("%s: Cannot open %s\n", filename, bios->blobs[romid].filename);
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
					printf("malloc failed\n");
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
				die("Unknown blob type\n");
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

static bool naomi_cart_LoadZip(char *filename)
{
	char *p = strrchr(filename, '/');
#ifdef _WIN32
	p = strrchr(p == NULL ? filename : p, '\\');
#endif
	if (p == NULL)
		p = filename;
	else
		p++;
	char game_name[128];
	strncpy(game_name, p, sizeof(game_name) - 1);
	game_name[sizeof(game_name) - 1] = 0;
	char *dot = strrchr(game_name, '.');
	if (dot != NULL)
		*dot = 0;

	int gameid = 0;
	for (; Games[gameid].name != NULL; gameid++)
		if (!stricmp(Games[gameid].name, game_name))
			break;
	if (Games[gameid].name == NULL)
	{
		printf("Unknown game %s\n", filename);
		return false;
	}

	struct Game *game = &Games[gameid];

	Archive *archive = OpenArchive(filename);
	if (archive != NULL)
		printf("Opened %s\n", filename);

	Archive *parent_archive = NULL;
	if (game->parent_name != NULL)
	{
	   strncpy(g_parent_name, game->parent_name, sizeof(game->parent_name));
	   std::string parent_path(g_roms_dir);
	   parent_path += "/";
	   parent_path += game->parent_name;
	   parent_archive = OpenArchive(parent_path.c_str());
	   if (parent_archive != NULL)
		  printf("Opened %s\n", game->parent_name);
	}

	if (archive == NULL && parent_archive == NULL)
	{
		if (game->parent_name != NULL)
			printf("Cannot open %s or %s\n", filename, game->parent_name);
		else
			printf("Cannot open %s\n", filename);
		return false;
	}

	const char *bios = "naomi";
	if (game->bios != NULL)
	   bios = game->bios;
	u32 region_flag = settings.dreamcast.region;
	if (region_flag > game->region_flag)
	   region_flag = game->region_flag;
	if (!naomi_LoadBios(bios, archive, parent_archive, region_flag))
	{
	   printf("Warning: Region %d bios not found in %s\n", settings.dreamcast.region, bios);
	   if (!naomi_LoadBios(bios, archive, parent_archive, -1))
	   {
		  // If a specific BIOS is needed for this game, fail.
		  if (game->bios != NULL || !bios_loaded)
		  {
			 printf("Error: cannot load BIOS. Exiting\n");
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
		die("Unsupported cartridge type\n");
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
			printf("Copied: %x bytes from %07x to %07x\n", game->blobs[romid].length, game->blobs[romid].src_offset, game->blobs[romid].offset);
		}
		else
		{
			ArchiveFile* file = NULL;
			if (archive != NULL)
				file = archive->OpenFile(game->blobs[romid].filename);
			if (file == NULL && parent_archive != NULL)
				file = parent_archive->OpenFile(game->blobs[romid].filename);
			if (!file) {
				printf("%s: Cannot open %s\n", filename, game->blobs[romid].filename);
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
				printf("Mapped %s: %x bytes at %07x\n", game->blobs[romid].filename, read, game->blobs[romid].offset);
			}
			else if (game->blobs[romid].blob_type == InterleavedWord)
			{
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					delete file;
					goto error;
				}
				u32 read = file->Read(buf, game->blobs[romid].length);
				u16 *to = (u16 *)CurrentCartridge->GetPtr(game->blobs[romid].offset, len);
				u16 *from = (u16 *)buf;
				for (int i = game->blobs[romid].length / 2; --i >= 0; to++)
					*to++ = *from++;
				free(buf);
				printf("Mapped %s: %x bytes (interleaved word) at %07x\n", game->blobs[romid].filename, read, game->blobs[romid].offset);
			}
			else if (game->blobs[romid].blob_type == Key)
			{
				u8 *buf = (u8 *)malloc(game->blobs[romid].length);
				if (buf == NULL)
				{
					printf("malloc failed\n");
					delete file;
					goto error;
				}
				u32 read = file->Read(buf, game->blobs[romid].length);
				CurrentCartridge->SetKeyData(buf);
				printf("Loaded %s: %x bytes cart key\n", game->blobs[romid].filename, read);
			}
			else if (game->blobs[romid].blob_type == Eeprom)
			{
			    naomi_default_eeprom = (u8 *)malloc(game->blobs[romid].length);
			    if (naomi_default_eeprom == NULL)
			    {
			       printf("malloc failed\n");
					delete file;
			       goto error;
			    }
				u32 read = file->Read(naomi_default_eeprom, game->blobs[romid].length);
				printf("Loaded %s: %x bytes default eeprom\n", game->blobs[romid].filename, read);
			}
			else
				die("Unknown blob type\n");
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
	printf("NAOMI GAME ID [%s]\n", naomi_game_id);

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
   const char *pdot = strrchr(file, '.');

   if (pdot == NULL
		 || (strcmp(pdot, ".zip") && strcmp(pdot, ".ZIP")
			 && strcmp(pdot, ".7z") && strcmp(pdot, ".7Z")))
	  // Not a ZIP file so it has to be a Naomi game
	  return DC_PLATFORM_NAOMI;

   const char *p = strrchr(file, '/');
#ifdef _WIN32
   p = strrchr(p == NULL ? file : p, '\\');
#endif
   if (p == NULL)
	  p = file;
   else
	  p++;
   char game_name[128];
   strncpy(game_name, p, pdot - p);
   game_name[pdot - p] = 0;

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

bool naomi_cart_LoadRom(char* file, char *s, size_t len)
{
	printf("nullDC-Naomi rom loader v1.2\n");
	printf("File: %s\n", file);

	if (CurrentCartridge != NULL)
	{
		delete CurrentCartridge;
		CurrentCartridge = NULL;
	}

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

	char *pdot = strrchr(file, '.');

	if (pdot != NULL
			&& (!strcmp(pdot, ".zip") || !strcmp(pdot, ".ZIP")
				|| !strcmp(pdot, ".7z") || !strcmp(pdot, ".7Z")))
		return naomi_cart_LoadZip(file);

	// Try to load BIOS from naomi.zip
	if (!naomi_LoadBios("naomi", NULL, NULL, settings.dreamcast.region))
	{
	   printf("Warning: Region %d bios not found in naomi.zip\n", settings.dreamcast.region);
	   if (!naomi_LoadBios("naomi", NULL, NULL, -1))
	   {
		  if (!bios_loaded)
		  {
			 printf("Error: cannot load BIOS. Exiting\n");
			 return false;
		  }
	   }
	}

	u8* RomPtr;
	u32 RomSize;

	if (pdot != NULL && (!strcmp(pdot, ".lst") || !strcmp(pdot, ".LST")))
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
		   printf("+Parsing was unsuccessful, there is something wrong with your lst file\n");
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

	   printf("+Loading naomi rom : %s\n", line);

	   strcpy(s, line);

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
			   printf("Warning: invalid line in .lst file: %s\n", line);
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

	printf("+%lu romfiles, %.2f MB set size, %.2f MB set address space\n", files.size(), setsize / 1024.f / 1024.f, RomSize / 1024.f / 1024.f);

	if (RomCacheMap)
	{
		RomCacheMapCount = 0;
		delete[] RomCacheMap;
	}

	RomCacheMapCount = (u32)files.size();
	RomCacheMap = new fd_t[files.size()]();

	//Allocate space for the ram, so we are sure we have a segment of continius ram
#ifdef _WIN32
	RomPtr = (u8*)VirtualAlloc(0, RomSize, MEM_RESERVE, PAGE_NOACCESS);
#else
	RomPtr = (u8*)mmap(0, RomSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#endif

	verify(RomPtr != 0);
	verify(RomPtr != (void*)-1);

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
		   printf("-Unable to read file %s: error %d\n", t, errno);
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
#ifdef _WIN32
	verify(VirtualFree(RomPtr, 0, MEM_RELEASE));
#else
	munmap(RomPtr, RomSize);
#endif

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
			
#ifdef _WIN32
			bool mapped = RomDest == VirtualAlloc(RomDest, fsize[i], MEM_RESERVE, PAGE_NOACCESS);
#else
			bool mapped = RomDest == (u8*)mmap(RomDest, RomSize, PROT_NONE, MAP_PRIVATE, 0, 0);
#endif

			if (!mapped)
			{
			   printf("-Mapping RAM FAILED @ %08x size %x\n", fstart[i], fsize[i]);
			   return false;
			}
		}
		else
		{
			//printf("-Mapping \"%s\" at 0x%08X, size 0x%08X\n", files[i].c_str(), fstart[i], fsize[i]);
#ifdef _WIN32
			bool mapped = RomDest == MapViewOfFileEx(RomCacheMap[i], FILE_MAP_READ, 0, 0, fsize[i], RomDest);
#else
			bool mapped = RomDest == mmap(RomDest, fsize[i], PROT_READ, MAP_PRIVATE, RomCacheMap[i], 0 );
#endif
			if (!mapped)
			{
			   printf("-Mapping ROM FAILED: %s @ %08x size %x\n", files[i].c_str(), fstart[i], fsize[i]);
			   return false;
			}
		}
	}

	//done :)
	printf("\nMapped ROM Successfully !\n\n");

	CurrentCartridge = new DecryptedCartridge(RomPtr, RomSize);
	strcpy(naomi_game_id, CurrentCartridge->GetGameId().c_str());
	printf("NAOMI GAME ID [%s]\n", naomi_game_id);

	return true;
}

extern char *game_data;
extern char eeprom_file[PATH_MAX];

bool naomi_cart_SelectFile(char *s, size_t len)
{
	if (!naomi_cart_LoadRom(game_data, s, len))
	{
	   printf("Cannot load %s: error %d\n", game_data, errno);
	   return false;
	}

	printf("EEPROM file : %s\n", eeprom_file);

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
	free(RomPtr);
}

bool Cartridge::Read(u32 offset, u32 size, void* dst)
{
	offset &= 0x1FFFFFFF;
	if (offset >= RomSize || (offset + size) > RomSize)
	{
		static u32 ones = 0xffffffff;

		// Makes Outtrigger boot
		EMUERROR("offset %d > %d\n", offset, RomSize);
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
	EMUERROR("Invalid write @ %08x data %x\n", offset, data);
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
		EMUERROR("Error: DmaOffset >= RomSize\n");
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
	case 0x3c:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_3c | (NaomiDataRead ? 0 : -1); //pretend the board isn't there for the bios
	case 0x40:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_40;
	case 0x44:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_44;
	case 0x48:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_48;

		//These are known to be valid on normal ROMs and DIMM board
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
		printf("naomi COMM offs READ: %X, %d\n", address, size);
		return CommOffset;
		#endif
	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		printf("naomi COMM data read: %X, %d\n", CommOffset, size);
		if (CommSharedMem)
		{
			return CommSharedMem[CommOffset&0xF];
		}
		#endif
		return 1;


		//This should be valid
	case NAOMI_DMA_OFFSETH_addr&255:
		return DmaOffset>>16;
	case NAOMI_DMA_OFFSETL_addr&255:
		return DmaOffset&0xFFFF;

	case NAOMI_BOARDID_WRITE_addr&255:
		EMUERROR("naomi ReadBoardId: %X, %d", address, size);
		return 1;

	case 0x04C:
		EMUERROR("naomi GD? READ: %X, %d", address, size);
		return reg_dimm_4c;

	case 0x18:
		printf("naomi reg 0x18 : returning random data\n");
		return 0x4000^rand();
		break;

	default: break;
	}
	//EMUERROR("naomi?WTF? ReadMem: %X, %d", address, size);

	return 0xFFFF;
}

void NaomiCartridge::WriteMem(u32 address, u32 data, u32 size)
{
	//	printf("+naomi WriteMem: %X <= %X, %d\n", address, data, size);
	switch(address & 255)
	{
	case 0x3c:
		 if (0x1E03==data)
		 {
			 /*
			 if (!(reg_dimm_4c&0x100))
				asic_RaiseInterrupt(holly_EXP_PCI);
			 reg_dimm_4c|=1;*/
		 }
		 reg_dimm_3c=data;
		 EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		 return;

	case 0x40:
		reg_dimm_40=data;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;
	case 0x44:
		reg_dimm_44=data;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;
	case 0x48:
		reg_dimm_48=data;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
		return;

	case 0x4C:
		if (data&0x100)
		{
			asic_CancelInterrupt(holly_EXP_PCI);
			naomi_updates=100;
		}
		else if ((data&1)==0)
		{
			/*FILE* ramd=fopen("c:\\ndc.ram.bin","wb");
			fwrite(mem_b.data,1,RAM_SIZE,ramd);
			fclose(ramd);*/
			naomi_process(reg_dimm_3c,reg_dimm_40,reg_dimm_44,reg_dimm_48);
		}
		reg_dimm_4c=data&~0x100;
		EMUERROR("naomi GD? Write: %X <= %X, %d", address, data, size);
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
		{
			DmaCount=data;
		}
		return;
	case NAOMI_BOARDID_WRITE_addr&255:
		NaomiGameIDWrite((u16)data);
		return;

		//What should i do to emulate 'nothing' ?
	case NAOMI_COMM_OFFSET_addr&255:
#ifdef NAOMI_COMM
		printf("naomi COMM ofset Write: %X <= %X, %d\n", address, data, size);
		CommOffset=data&0xFFFF;
#endif
		return;

	case NAOMI_COMM_DATA_addr&255:
		#ifdef NAOMI_COMM
		printf("naomi COMM data Write: %X <= %X, %d\n", CommOffset, data, size);
		if (CommSharedMem)
		{
			CommSharedMem[CommOffset&0xF]=data;
		}
		#endif
		return;

		//This should be valid
	case NAOMI_BOARDID_READ_addr&255:
		EMUERROR("naomi WriteMem: %X <= %X, %d", address, data, size);
		return;

	default: break;
	}
	EMUERROR("naomi?WTF? WriteMem: %X <= %X, %d", address, data, size);
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
		EMUERROR("Invalid read @ %08x\n", offset);
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
