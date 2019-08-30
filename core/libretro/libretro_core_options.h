#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CORE_OPTION_NAME "reicast"

#define COLORS_STRING \
      {"BLACK 02",          "Black" }, \
      {"BLUE 03",           "Blue" }, \
      {"LIGHT_BLUE 04",     "Light Blue" }, \
      {"GREEN 05",          "Green" }, \
      {"CYAN 06",           "Cyan" }, \
      {"CYAN_BLUE 07",      "Cyan Blue" }, \
      {"LIGHT_GREEN 08",    "Light Green" }, \
      {"CYAN_GREEN 09",     "Cyan Green" }, \
      {"LIGHT_CYAN 10",     "Light Cyan" }, \
      {"RED 11",            "Red" }, \
      {"PURPLE 12",         "Purple" }, \
      {"LIGHT_PURPLE 13",   "Light Purple" }, \
      {"YELLOW 14",         "Yellow" }, \
      {"GRAY 15",           "Gray" }, \
      {"LIGHT_PURPLE_2 16", "Light Purple (2)" }, \
      {"LIGHT_GREEN_2 17",  "Light Green (2)" }, \
      {"LIGHT_GREEN_3 18",  "Light Green (3)" }, \
      {"LIGHT_CYAN_2 19",   "Light Cyan (2)" }, \
      {"LIGHT_RED_2 20",    "Light Red (2)" }, \
      {"MAGENTA 21",        "Magenta" }, \
      {"LIGHT_PURPLE_2 22", "Light Purple (2)" }, \
      {"LIGHT_ORANGE 23",   "Light Orange" }, \
      {"ORANGE 24",         "Orange" }, \
      {"LIGHT_PURPLE_3 25", "Light Purple (3)" }, \
      {"LIGHT_YELLOW 26",   "Light Yellow" }, \
      {"LIGHT_YELLOW_2 27", "Light Yellow (2)" }, \
      {"WHITE 28",          "White" }, \
      { NULL, NULL },

#define VMU_SCREEN_PARAMS(num) \
{ \
   CORE_OPTION_NAME "_vmu" #num "_screen_display", \
   "VMU Screen " #num " Display", \
   "", \
   { \
      { "disabled", NULL }, \
      { "enabled",  NULL }, \
      { NULL, NULL }, \
   }, \
   "disabled", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_screen_position", \
"VMU Screen " #num " Position", \
"", \
{ \
   { "Upper Left",  NULL }, \
   { "Upper Right", NULL }, \
   { "Lower Left",  NULL }, \
   { "Lower Right", NULL }, \
   { NULL, NULL }, \
}, \
   "Upper Left", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_screen_size_mult", \
"VMU Screen " #num " Size", \
"", \
{ \
   { "1x", NULL }, \
   { "2x", NULL }, \
   { "3x", NULL }, \
   { "4x", NULL }, \
   { "5x", NULL }, \
   { NULL, NULL }, \
}, \
   "1x", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_pixel_on_color", \
"VMU Screen " #num " Pixel On Color", \
"", \
{ \
   { "DEFAULT_ON 00",  "Default ON" }, \
   { "DEFAULT_OFF 01", "Default OFF" }, \
COLORS_STRING \
}, \
   "DEFAULT_ON 00", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_pixel_off_color", \
"VMU Screen " #num " Pixel Off Color", \
"", \
{ \
   { "DEFAULT_OFF 01", "Default OFF" }, \
   { "DEFAULT_ON 00",  "Default ON" }, \
COLORS_STRING \
}, \
   "DEFAULT_OFF 01", \
}, \
{ \
CORE_OPTION_NAME "_vmu" #num "_screen_opacity", \
"VMU Screen " #num " Opacity", \
"", \
{ \
   { "10%",  NULL }, \
   { "20%",  NULL }, \
   { "30%",  NULL }, \
   { "40%",  NULL }, \
   { "50%",  NULL }, \
   { "60%",  NULL }, \
   { "70%",  NULL }, \
   { "80%",  NULL }, \
   { "90%",  NULL }, \
   { "100%", NULL }, \
   { NULL,   NULL }, \
}, \
   "100%", \
},

#define LIGHTGUN_PARAMS(num) \
{ \
CORE_OPTION_NAME "_lightgun" #num "_crosshair", \
"Gun Crosshair " #num " Display", \
"", \
{ \
   { "disabled", NULL }, \
   { "White",    NULL }, \
   { "Red",      NULL }, \
   { "Green",    NULL }, \
   { "Blue",     NULL }, \
   { NULL,       NULL }, \
}, \
   "disabled", \
},

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_definition option_defs_us[] = {
#if ((FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)) && defined(TARGET_NO_JIT)
   {
      CORE_OPTION_NAME "_cpu_mode",
      "CPU Mode (Restart)",
      "",
      {
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
         { "dynamic_recompiler", "Dynamic Recompiler" },
#endif
#ifdef TARGET_NO_JIT
         { "generic_recompiler", "Generic Recompiler" },
#endif
         { NULL, NULL },
      },
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
      "dynamic_recompiler",
#elif defined(TARGET_NO_JIT)
      "generic_recompiler",
#endif
   },
#endif
   {
      CORE_OPTION_NAME "_boot_to_bios",
      "Boot to BIOS (Restart)",
      "Boot directly into the Dreamcast BIOS menu.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_system",
      "System Type (Restart)",
      "",
      {
         { "auto",       "Auto" },
         { "dreamcast",  "Dreamcast" },
         { "naomi",      "NAOMI" },
         { "atomiswave", "Atomiswave" },
         { NULL, NULL },
      },
      "auto",
   },
   {
      CORE_OPTION_NAME "_hle_bios",
      "HLE BIOS",
      "Force use of high-level emulation BIOS.",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
#ifdef HAVE_OIT
   {
      CORE_OPTION_NAME "_oit_abuffer_size",
      "Accumulation Pixel Buffer Size (Restart)",
      "",
      {
         { "512MB", NULL },
         { "1GB",   NULL },
         { "2GB",   NULL },
         { NULL, NULL },
      },
      "512MB",
   },
#endif
   {
      CORE_OPTION_NAME "_internal_resolution",
      "Internal Resolution (Restart)",
      "Modify rendering resolution. Requires a restart.",
      {
         { "320x240",    NULL },
         { "640x480",    NULL },
         { "800x600",    NULL },
         { "960x720",    NULL },
         { "1024x768",   NULL },
         { "1280x720",   NULL },
         { "1280x960",   NULL },
         { "1280x1024",  NULL },
         { "1440x1080",  NULL },
         { "1600x900",   NULL },
         { "1600x1200",  NULL },
         { "1920x1080",  NULL },
         { "1920x1440",  NULL },
         { "2560x1440",  NULL },
         { "2560x1920",  NULL },
         { "2880x2160",  NULL },
         { "3200x2400",  NULL },
         { "3440x1400",  NULL },
         { "3840x2160",  NULL },
         { "3840x2880",  NULL },
         { "4096x2160",  NULL },
         { "4480x3360",  NULL },
         { "5120x3840",  NULL },
         { "5760x4320",  NULL },
         { "6400x4800",  NULL },
         { "7040x5280",  NULL },
         { "7680x5760",  NULL },
         { "8320x6240",  NULL },
         { "8960x6720",  NULL },
         { "9600x7200",  NULL },
         { "10240x7680", NULL },
         { "10880x8160", NULL },
         { "11520x8640", NULL },
         { "12160x9120", NULL },
         { "12800x9600", NULL },
         { NULL, NULL },
      },
#ifdef LOW_RES
      "320x240",
#else
      "640x480",
#endif
   },
   {
      CORE_OPTION_NAME "_screen_rotation",
      "Screen Orientation",
      "",
      {
         { "horizontal", "Horizontal" },
         { "vertical",   "Vertical" },
         { NULL, NULL },
      },
      "horizontal",
   },
   {
      CORE_OPTION_NAME "_alpha_sorting",
      "Alpha Sorting",
      "",
      {
         { "per-strip (fast, least accurate)", "Per-Strip (fast, least accurate)" },
         { "per-triangle (normal)",            "Per-Triangle (normal)" },
#ifdef HAVE_OIT
         { "per-pixel (accurate)",             "Per-Pixel (accurate, but slowest)" },
#endif
         { NULL, NULL },
      },
#if defined(LOW_END)
      "per-strip (fast, least accurate)",
#else
      "per-triangle (normal)",
#endif
   },
   {
      CORE_OPTION_NAME "_gdrom_fast_loading",
      "GDROM Fast Loading (inaccurate)",
      "Speeds up GD-ROM loading. NOTE: This option doesn't work with the HLE BIOS for now.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
#ifdef LOW_END
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_mipmapping",
      "Mipmapping",
      "",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_volume_modifier_enable",
      "Volume Modifier",
      "A Dreamcast GPU feature that is typically used by games to draw object shadows. This should normally be enabled - the performance impact is usually minimal to negligible.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_widescreen_hack",
      "Widescreen hack (Restart)",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_cable_type",
      "Cable Type",
      "",
      {
         { "TV (RGB)",       NULL },
         { "TV (Composite)", NULL },
         { "VGA (RGB)",      NULL },
         { NULL, NULL },
      },
#ifdef LOW_END
      "VGA (RGB)",
#else
      "TV (RGB)",
#endif
   },
   {
      CORE_OPTION_NAME "_broadcast",
      "Broadcast",
      "",
      {
         { "Default", NULL },
         { "PAL_M",   "PAL-M (Brazil)" },
         { "PAL_N",   "PAL-N (Argentina, Paraguay, Uruguay)" },
         { "NTSC",    NULL },
         { "PAL",     "PAL (World)" },
         { NULL, NULL },
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_framerate",
      "Framerate",
      "Affects how emulator interacts with frontend. 'Full Speed' - emulator returns control to RetroArch each time a frame has been rendered. 'Normal' - emulator returns control to RetroArch each time a V-blank interrupt is generated. 'Full Speed' should be used in most cases. 'Normal' may improve frame pacing on some systems, but can cause unresponsive input when screen is static (e.g. loading/pause screens).",
      {
         { "fullspeed", "Full Speed" },
         { "normal",    "Normal" },
         { NULL, NULL },
      },
      "fullspeed",
   },
   {
      CORE_OPTION_NAME "_region",
      "Region",
      "",
      {
         { "Default", NULL },
         { "Japan",   NULL },
         { "USA",     NULL },
         { "Europe",  NULL },
         { NULL, NULL },
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_language",
      "Language",
      "",
      {
         { "Default",  NULL },
         { "Japanese", NULL },
         { "English",  NULL },
         { "German",   NULL },
         { "French",   NULL },
         { "Spanish",  NULL },
         { "Italian",  NULL },
         { NULL, NULL },
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_div_matching",
      "DIV Matching (performance, less accurate)",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { "auto",     "Auto" },
         { NULL, NULL },
      },
#ifdef LOW_END
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_analog_stick_deadzone",
      "Analog Stick Deadzone",
      "",
      {
         { "0%",  NULL },
         { "5%",  NULL },
         { "10%", NULL },
         { "15%", NULL },
         { "20%", NULL },
         { "25%", NULL },
         { "30%", NULL },
         { NULL, NULL },
      },
      "15%",
   },
   {
      CORE_OPTION_NAME "_trigger_deadzone",
      "Trigger Deadzone",
      "",
      {
         { "0%",  NULL },
         { "5%",  NULL },
         { "10%", NULL },
         { "15%", NULL },
         { "20%", NULL },
         { "25%", NULL },
         { "30%", NULL },
         { NULL, NULL },
      },
      "0%",
   },
   {
      CORE_OPTION_NAME "_digital_triggers",
      "Digital Triggers",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_dsp",
      "Enable DSP",
      "Enable emulation of the Dreamcast's audio DSP (digital signal processor). Improves the accuracy of generated sound, but increases performance requirements.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
#ifdef LOW_END
      "disabled",
#else
      "enabled",
#endif
   },
#ifdef HAVE_TEXUPSCALE
   {
      CORE_OPTION_NAME "_texupscale",
      "Texture Upscaling (xBRZ)",
      "",
      {
         { "off", "disabled" },
         { "2x",  NULL },
         { "4x",  NULL },
         { "6x",  NULL },
         { NULL, NULL },
      },
      "off",
   },
   {
      CORE_OPTION_NAME "_texupscale_max_filtered_texture_size",
      "Texture Upscaling Max. Filtered Size",
      "",
      {
         { "256",  NULL },
         { "512",  NULL },
         { "1024", NULL },
         { NULL, NULL },
      },
      "256",
   },
#endif
   {
      CORE_OPTION_NAME "_enable_rtt",
      "Enable RTT (Render To Texture)",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_enable_rttb",
      "Enable RTT (Render To Texture) Buffer",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_render_to_texture_upscaling",
      "Render To Texture Upscaling",
      "",
      {
         { "1x", NULL },
         { "2x", NULL },
         { "3x", NULL },
         { "4x", NULL },
         { "8x", NULL },
         { NULL, NULL },
      },
      "1x",
   },
#if !defined(TARGET_NO_THREADS)
   {
      CORE_OPTION_NAME "_threaded_rendering",
      "Threaded Rendering (Restart)",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
#if defined(ANDROID) || defined(IOS) || defined(THREADED_RENDERING_DEFAULT)
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_synchronous_rendering",
      "Synchronous Rendering",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
#endif
   {
      CORE_OPTION_NAME "_frame_skipping",
      "Frame Skipping",
      "",
      {
         { "disabled",  NULL },
         { "1",         NULL },
         { "2",         NULL },
         { "3",         NULL },
         { "4",         NULL },
         { "5",         NULL },
         { "6",         NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_purupuru",
      "Purupuru Pack/Vibration Pack",
      "Enables controller force feedback.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_allow_service_buttons",
      "Allow NAOMI Service Buttons",
      "Enables SERVICE button for NAOMI, to enter cabinet settings.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_naomi_15khz_dipswitch",
      "Enable NAOMI 15KHz Dipswitch",
      "This can force display in 240p, 480i or no effect at all depending on the game.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_custom_textures",
      "Load Custom Textures",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_dump_textures",
      "Dump Textures",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_per_content_vmus",
      "Per-Game VMUs",
      "When disabled, all games share 4 VMU save files (A1, B1, C1, D1) located in RetroArch's system directory. The 'VMU A1' setting creates a unique VMU 'A1' file in RetroArch's save directory for each game that is launched. The 'All VMUs' setting creates 4 unique VMU files (A1, B1, C1, D1) for each game that is launched.",
      {
         { "disabled", NULL },
         { "VMU A1",   NULL },
         { "All VMUs", NULL },
         { NULL, NULL},
      },
      "disabled",
   },
   VMU_SCREEN_PARAMS(1)
   VMU_SCREEN_PARAMS(2)
   VMU_SCREEN_PARAMS(3)
   VMU_SCREEN_PARAMS(4)
   LIGHTGUN_PARAMS(1)
   LIGHTGUN_PARAMS(2)
   LIGHTGUN_PARAMS(3)
   LIGHTGUN_PARAMS(4)
   { NULL, NULL, NULL, {{0}}, NULL },
};

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

struct retro_core_option_definition option_defs_tr[] = {
#if ((FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)) && defined(TARGET_NO_JIT)
   {
      CORE_OPTION_NAME "_cpu_mode",
      "CPU Modu (Yeniden Başlatma Gerektirir)",
      "",
      {
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
         { "dynamic_recompiler", "Dynamic Recompiler" },
#endif
#ifdef TARGET_NO_JIT
         { "generic_recompiler", "Generic Recompiler" },
#endif
         { NULL, NULL },
      },
#if (FEAT_SHREC == DYNAREC_JIT && HOST_CPU == CPU_X86) || (HOST_CPU == CPU_ARM) || (HOST_CPU == CPU_ARM64) || (HOST_CPU == CPU_X64)
      "dynamic_recompiler",
#elif defined(TARGET_NO_JIT)
      "generic_recompiler",
#endif
   },
#endif
   {
      CORE_OPTION_NAME "_boot_to_bios",
      "BIOS'a önyükleme (Yeniden Başlatma Gerektirir)",
      "Doğrudan Dreamcast BIOS menüsüne önyükleme yapın.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_system",
      "Sistem Tipi (Yeniden Başlatma Gerektirir)",
      "",
      {
         { "auto",       "Otomatik" },
         { "dreamcast",  "Dreamcast" },
         { "naomi",      "NAOMI" },
         { "atomiswave", "Atomiswave" },
         { NULL, NULL },
      },
      "auto",
   },
   {
      CORE_OPTION_NAME "_hle_bios",
      "HLE BIOS",
      "Üst düzey öykünmüş BIOS kullanımını zorla.",
      {
         { "disabled",  NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled",
   },
#ifdef HAVE_OIT
   {
      CORE_OPTION_NAME "_oit_abuffer_size",
      "Birikim Piksel Arabellek Boyutu (Yeniden Başlatma Gerektirir)",
      "",
      {
         { "512MB", NULL },
         { "1GB",   NULL },
         { "2GB",   NULL },
         { NULL, NULL },
      },
      "512MB",
   },
#endif
   {
      CORE_OPTION_NAME "_internal_resolution",
      "Dahili Çözünürlük (Yeniden Başlat Gerektirir)",
      "Render çözünürlüğünü değiştirin. Yeniden başlatma gerektirir.",
      {
         { "320x240",    NULL },
         { "640x480",    NULL },
         { "800x600",    NULL },
         { "960x720",    NULL },
         { "1024x768",   NULL },
         { "1280x720",   NULL },
         { "1280x960",   NULL },
         { "1280x1024",  NULL },
         { "1440x1080",  NULL },
         { "1600x900",   NULL },
         { "1600x1200",  NULL },
         { "1920x1080",  NULL },
         { "1920x1440",  NULL },
         { "2560x1440",  NULL },
         { "2560x1920",  NULL },
         { "2880x2160",  NULL },
         { "3200x2400",  NULL },
         { "3440x1400",  NULL },
         { "3840x2160",  NULL },
         { "3840x2880",  NULL },
         { "4096x2160",  NULL },
         { "4480x3360",  NULL },
         { "5120x3840",  NULL },
         { "5760x4320",  NULL },
         { "6400x4800",  NULL },
         { "7040x5280",  NULL },
         { "7680x5760",  NULL },
         { "8320x6240",  NULL },
         { "8960x6720",  NULL },
         { "9600x7200",  NULL },
         { "10240x7680", NULL },
         { "10880x8160", NULL },
         { "11520x8640", NULL },
         { "12160x9120", NULL },
         { "12800x9600", NULL },
         { NULL, NULL },
      },
#ifdef LOW_RES
      "320x240",
#else
      "640x480",
#endif
   },
   {
      CORE_OPTION_NAME "_screen_rotation",
      "Ekran Yönü",
      "",
      {
         { "horizontal", "Yatay" },
         { "vertical",   "Dikey" },
         { NULL, NULL },
      },
      "horizontal",
   },
   {
      CORE_OPTION_NAME "_alpha_sorting",
      "Alfa Sıralama",
      "",
      {
         { "per-strip (fast, least accurate)", "Şerit Başına (hızlı, en az doğru)" },
         { "per-triangle (normal)",            "Üçgen Başına (normal)" },
#ifdef HAVE_OIT
         { "per-pixel (accurate)",             "Piksel Başına (doğru, ancak en yavaş)" },
#endif
         { NULL, NULL },
      },
#if defined(LOW_END)
      "per-strip (fast, least accurate)",
#else
      "per-triangle (normal)",
#endif
   },
   {
      CORE_OPTION_NAME "_gdrom_fast_loading",
      "GDROM Hızlı Yükleme (kusurlu)",
      "GD-ROM yüklemesini hızlandırır. NOT: Bu seçenek şimdilik HLE BIOS ile çalışmıyor.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
#ifdef LOW_END
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_mipmapping",
      "Mipmapping",
      "",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_volume_modifier_enable",
      "Hacim Değiştirici",
      "Nesne gölgeleri çizmek için genellikle oyunlar tarafından kullanılan bir Dreamcast GPU özelliği. Bu normalde etkinleştirilmelidir - performansın etkisi ihmal edilebilir düzeyde genellikle minimum düzeydedir.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_widescreen_hack",
      "Geniş ekran kesmesi (Yeniden Başlatma Gerektirir)",
      "",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_cable_type",
      "Kablo Tipi",
      "",
      {
         { "TV (RGB)",       NULL },
         { "TV (Composite)", NULL },
         { "VGA (RGB)",      NULL },
         { NULL, NULL },
      },
#ifdef LOW_END
      "VGA (RGB)",
#else
      "TV (RGB)",
#endif
   },
   {
      CORE_OPTION_NAME "_broadcast",
      "Yayın",
      "",
      {
         { "Default", NULL },
         { "PAL_M",   "PAL-M (Brazil)" },
         { "PAL_N",   "PAL-N (Argentina, Paraguay, Uruguay)" },
         { "NTSC",    NULL },
         { "PAL",     "PAL (World)" },
         { NULL, NULL },
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_framerate",
      "Kare Hızı",
      "Emülatörün ön uçla nasıl etkileşimde bulunduğunu etkiler. 'Tam Hız' - emülatör, bir kare oluşturulduğunda, kontrolü RetroArch'a geri döndürür. 'Normal' - emülatör, V-blank kesmesi her üretildiğinde kontrolü RetroArch'a döndürür. Çoğu durumda 'Tam Hız' kullanılmalıdır. 'Normal' bazı sistemlerde kare ilerleme hızını iyileştirebilir, ancak ekran statik olduğunda (örneğin, yükleme/duraklatma ekranları) yanıt vermeyen girişe neden olabilir.",
      {
         { "fullspeed", "Tam Hız" },
         { "normal",    "Normal" },
         { NULL, NULL },
      },
      "fullspeed",
   },
   {
      CORE_OPTION_NAME "_region",
      "Bölge",
      "",
      {
         { "Default", "Varsayılan" },
         { "Japan",   NULL },
         { "USA",     NULL },
         { "Europe",  NULL },
         { NULL, NULL },
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_language",
      "Dil",
      "",
      {
         { "Default",  "Varsayılan" },
         { "Japanese", NULL },
         { "English",  NULL },
         { "German",   NULL },
         { "French",   NULL },
         { "Spanish",  NULL },
         { "Italian",  NULL },
         { NULL, NULL },
      },
      "Default",
   },
   {
      CORE_OPTION_NAME "_div_matching",
      "DIV Eşleştirme (performans, daha az doğru)",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { "auto",     "Otomatik" },
         { NULL, NULL },
      },
#ifdef LOW_END
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_analog_stick_deadzone",
      "Analog Çubuğu Ölü Bölge",
      "",
      {
         { "0%",  NULL },
         { "5%",  NULL },
         { "10%", NULL },
         { "15%", NULL },
         { "20%", NULL },
         { "25%", NULL },
         { "30%", NULL },
         { NULL, NULL },
      },
      "15%",
   },
   {
      CORE_OPTION_NAME "_trigger_deadzone",
      "Tetik Ölü Bölge",
      "",
      {
         { "0%",  NULL },
         { "5%",  NULL },
         { "10%", NULL },
         { "15%", NULL },
         { "20%", NULL },
         { "25%", NULL },
         { "30%", NULL },
         { NULL, NULL },
      },
      "0%",
   },
   {
      CORE_OPTION_NAME "_digital_triggers",
      "Dijital Tetikleyiciler",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_dsp",
      "DSP'yi Etkinleştir",
      "Dreamcast'in ses DSP'sinin (dijital sinyal işlemcisi) öykünmesini etkinleştirin. Üretilen sesin doğruluğunu arttırır, ancak performans gereksinimlerini artırır.",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
#ifdef LOW_END
      "disabled",
#else
      "enabled",
#endif
   },
#ifdef HAVE_TEXUPSCALE
   {
      CORE_OPTION_NAME "_texupscale",
      "Doku Büyütme (xBRZ)",
      "",
      {
         { "off", "Devre Dışı" },
         { "2x",  NULL },
         { "4x",  NULL },
         { "6x",  NULL },
         { NULL, NULL },
      },
      "off",
   },
   {
      CORE_OPTION_NAME "_texupscale_max_filtered_texture_size",
      "Doku Yükseltme Maks. Filtre boyutu",
      "",
      {
         { "256",  NULL },
         { "512",  NULL },
         { "1024", NULL },
         { NULL, NULL },
      },
      "256",
   },
#endif
   {
      CORE_OPTION_NAME "_enable_rtt",
      "RTT'yi etkinleştir (Dokuya Render'i)",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_enable_rttb",
      "RTT'yi etkinleştirme (Dokuya Render'i) ara belleği",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_render_to_texture_upscaling",
      "Doku Yükseltme İşlemine Render",
      "",
      {
         { "1x", NULL },
         { "2x", NULL },
         { "3x", NULL },
         { "4x", NULL },
         { "8x", NULL },
         { NULL, NULL },
      },
      "1x",
   },
#if !defined(TARGET_NO_THREADS)
   {
      CORE_OPTION_NAME "_threaded_rendering",
      "İşlem Parçacığı Renderlama (Yeniden Başlatma Gerektirir)",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
#if defined(ANDROID) || defined(IOS) || defined(THREADED_RENDERING_DEFAULT)
      "enabled",
#else
      "disabled",
#endif
   },
   {
      CORE_OPTION_NAME "_synchronous_rendering",
      "Senkronize İşleme",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
#endif
   {
      CORE_OPTION_NAME "_frame_skipping",
      "Kare Atlama",
      "",
      {
         { "disabled",  "Devre Dışı" },
         { "1",         NULL },
         { "2",         NULL },
         { "3",         NULL },
         { "4",         NULL },
         { "5",         NULL },
         { "6",         NULL },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_purupuru",
      "Purupuru Paketi / Titreşim Paketi",
      "Denetleyici geri bildirimini etkinleştirir.",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "enabled",
   },
   {
      CORE_OPTION_NAME "_allow_service_buttons",
      "Allow NAOMI Service Buttons",
      "Kabin ayarlarına girmek için NAOMI'nin SERVİS düğmesini etkinleştirir.",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_enable_naomi_15khz_dipswitch",
      "NAOMI 15KHz Dipswitch'i etkinleştir",
      "Bu, 240p, 480i'de gösterimi zorlayabilir veya oyuna bağlı olarak hiçbir etkisi olmayabilir.",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_custom_textures",
      "Özel Dokular Yükle",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_dump_textures",
      "Dokuları Göm",
      "",
      {
         { "disabled", "Devre Dışı" },
         { "enabled",  "Etkin" },
         { NULL, NULL },
      },
      "disabled",
   },
   {
      CORE_OPTION_NAME "_per_content_vmus",
      "Oyun Başına VMU'lar",
      "Devre dışı bırakıldığında, tüm oyunlar RetroArch'ın sistem dizininde bulunan 4 VMU kaydetme dosyasını (A1, B1, C1, D1) paylaşır. 'VMU A1' ayarı, RetroArch'ın başlattığı her oyun için kaydetme dizininde benzersiz bir VMU 'A1' dosyası oluşturur. 'Tüm VMU'lar' ayarı, başlatılan her oyun için 4 benzersiz VMU dosyası (A1, B1, C1, D1) oluşturur.",
      {
         { "disabled", "Devre Dışı" },
         { "VMU A1",   NULL },
         { "All VMUs", "Tüm VMU'lar" },
         { NULL, NULL},
      },
      "disabled",
   },
   VMU_SCREEN_PARAMS(1)
   VMU_SCREEN_PARAMS(2)
   VMU_SCREEN_PARAMS(3)
   VMU_SCREEN_PARAMS(4)
   LIGHTGUN_PARAMS(1)
   LIGHTGUN_PARAMS(2)
   LIGHTGUN_PARAMS(3)
   LIGHTGUN_PARAMS(4)
   { NULL, NULL, NULL, {{0}}, NULL },
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   option_defs_tr, /* RETRO_LANGUAGE_TURKISH */
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version == 1))
   {
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
   }
   else
   {
      size_t i;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string
             * > Note: flycast is unusual in that we have to
             *   support core options with only one value
             *   (the number of '_cpu_mode' options depends
             *   upon compiler flags...) */
            if (num_values > 0)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[i].key   = key;
         variables[i].value = values_buf[i];
      }
      
      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif
