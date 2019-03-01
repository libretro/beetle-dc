/*
 * Notes
 * Games that need certain settings activated in order to function properly:
 *
 * update mode type -
 * * Street Fighter III Double Impact (would run at choppy framerate otherwise)
 *
 *
 * aica_interrupt_hack -
 * * Street Fighter Alpha 3 (would hang at startup screen otherwise
 */

struct game_type
{
   char product_number[11];
   int updatemode_type;     /* -1, make no decision, 0 = update mode normal, 1 = update mode fullspeed */
   int dynarec_type;        /* -1, make no decision, 0 = dynamic recompiler, 1 = generic_recompiler */
   int aica_interrupt_hack; /* -1, make no decision, 0 = normal, 1 = enable hack */
   int alpha_sort_mode;     /* -1, make no decision */
   int translucentPolygonDepthMask; /* -1, make no decision */
   int rendertotexturebuffer;       /* -1, make no decision */
   int disable_div;                 /* -1, make no decision */
   float extra_depth_scale;			/* 1, default */
};

struct game_type_naomi
{
   char product_number[128];
   int updatemode_type;     /* -1, make no decision, 0 = update mode normal, 1 = update mode fullspeed */
   int dynarec_type;        /* -1, make no decision, 0 = dynamic recompiler, 1 = generic_recompiler */
   int aica_interrupt_hack; /* -1, make no decision, 0 = normal, 1 = enable hack */
   int alpha_sort_mode;     /* -1, make no decision */
   int translucentPolygonDepthMask; /* -1, make no decision */
   int rendertotexturebuffer;       /* -1, make no decision */
   int disable_div;                 /* -1, make no decision */
   int jamma_setup;                 /* -1, make no decision,
									   0 = normal 2P setup,
									   1 = 4 players setup
									   2 = 2 I/O boards w/ rotary encoder (or Track ball for AtomisWave)
									   3 = 12bits/player, 16 drivers out for Sega Marine Fishing
									   4 = 2 I/O boards, 4 players for Ring Out 4x4
									   5 = 3 analog axes (AtomisWave)
									   6 = 2 light guns (AtomisWave)
									   */
   float extra_depth_scale;         /* 1, default */
   InputDescriptors *game_inputs;
};

static struct game_type lut_games[] = 
{
   /* Update mode fullspeed */
   { "T1210N    ",  1, -1, -1, -1, -1, -1,  -1, 1  },                /* Street Fighter III Double Impact */

   /* Fallback to generic recompiler */

   /* Alpha sort mode */
   { "MK-5100050", -1, -1, -1,  1, -1, -1,  -1, 1  },                /* Sonic Adventure */

   /* Translucent Polygon Depth Mask */
   { "RDC-0057  ", -1, -1, -1, -1,  1, -1,  -1, 1  },                /* Cosmic Smash */
   { "HDR-0176  ", -1, -1, -1, -1,  1, -1,  -1, 1  },                /* Cosmic Smash */

   /* Render to texture buffer */
   { "T40205N   ", -1, -1, -1, -1, -1,  1,  -1, 1  },                /* Tony Hawk's Pro Skater 1 (USA) */
   { "T13006N   ", -1, -1, -1, -1, -1,  1,  -1, 1  },                /* Tony Hawk's Pro Skater 2 (USA) */
   { "T13008D",    -1, -1, -1, -1, -1,  1,  -1, 1  },                /* Tony Hawk's Pro Skater 2 (USA) */
   { "T23002N   ", -1, -1, -1, -1, -1,  1,  -1, 1  },                /* Star Wars Episode I: Jedi Power Battle (USA) */
   { "MK-51052  ", -1, -1, -1, -1, -1,  1,  -1, 1  },                /* Skies of Arcadia */

   /* Disable DIV matching */
   { "T40216N   ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Surf Rocket Racers */
   { "T23001D   ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Star Wars - Episode I - Racer (United Kingdom) */
   { "T23001N   ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Star Wars - Episode I - Racer (USA) */
   { "T30701D50 ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Pro Pinball Trilogy */
   { "T15112N   ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Demolition Racer */
   { "T7012D    ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Record of Lodoss War (EU) */
   { "T40218N   ", -1, -1, -1, -1, -1,  -1,  1, 1  },                /* Record of Lodoss War (USA) */

   /* Extra depth scaling */
   { "MK-51182  ", -1, -1, -1, -1, -1,  -1,  1, 10000.f },           /* NHL 2K2 */
};

extern InputDescriptors gunsur2_inputs;
extern InputDescriptors ftspeed_inputs;
extern InputDescriptors maxspeed_inputs;

static struct game_type_naomi lut_games_naomi[] = 
{
   /* Div matching disabled */
   { "METAL SLUG 6"                      , -1, -1, -1, -1, -1, -1,  1, -1,  1 },                /* Metal Slug 6 */
   { "WAVE RUNNER GP"                    , -1, -1, -1, -1, -1, -1,  1, -1,  1 },                /* WaveRunner GP */

   /* Extra Depth Scaling */
   { "SAMURAI SPIRITS 6"                 , -1, -1, -1, -1, -1, -1,  -1, -1, 1e26 },             /* Samurai Shodown VI */

   /* Translucent Polygon Depth Mask */
   { "COSMIC SMASH IN JAPAN"             , -1, -1, -1, -1,  1, -1,  -1, -1, 1 },                /* Cosmic Smash */

   /* Alternate Jamma I/O Setup */
   { "POWER STONE 2 JAPAN"               , -1, -1, -1, -1, -1, -1,  -1,  1, 1 },                /* Power Stone 2 (4 players, also needs to be set in service menu) */
   { "SHOOTOUT POOL"                     , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Shootout Pool: rotary encoders */
   { "SHOOTOUT POOL MEDAL"               , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Shootout Pool The Medal: rotary encoders */
   { "DYNAMIC GOLF"                      , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Virtua Golf/Dynamic Golf: rotary encoders */
   { "CRACKIN'DJ  ver JAPAN"             , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Crackin'DJ: rotary encoders */
   { "CRACKIN'DJ PART2  ver JAPAN"       , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Crackin'DJ 2: rotary encoders */
   { "OUTTRIGGER     JAPAN"              , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Outtrigger: rotary encoders */
   { "SEGA MARINE FISHING JAPAN"         , -1, -1, -1, -1, -1, -1,  -1,  3, 1 },                /* Sega Marine Fishing */
   { "RINGOUT 4X4 JAPAN"                 , -1, -1, -1, -1, -1, -1,  -1,  4, 1 },                /* Ring Out 4x4 (4 players, also needs to be set in service menu) */
   { "Sports Shooting USA"               , -1, -1, -1, -1, -1, -1,  -1,  6, 1 },                /* Sports Shooting USA (light guns) */
   { "SEGA CLAY CHALLENGE"               , -1, -1, -1, -1, -1, -1,  -1,  6, 1 },                /* Sega Clay Challenge (light guns) */
   { "EXTREME HUNTING"                   , -1, -1, -1, -1, -1, -1,  -1,  6, 1 },                /* Extreme Hunting (light guns) */
   { "FASTER THAN SPEED"                 , -1, -1, -1, -1, -1, -1,  -1,  5, 1, &ftspeed_inputs },/* Faster Than Speed (analog axes) */
   { "MAXIMUM SPEED"                     , -1, -1, -1, -1, -1, -1,  -1,  5, 1, &maxspeed_inputs },/* Maximum Speed (analog axes) */
   { "BLOPON"                            , -1, -1, -1, -1, -1, -1,  -1,  5, 1 },                /* Block Pong (analog axes) */
   { "BASS FISHING SIMULATOR VER.A"      , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Sega Bass Fishing Challenge (Track-ball) */
   { "DRIVE"                             , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* WaiWai Drive */
   { "KICK '4' CASH"                     , -1, -1, -1, -1, -1, -1,  -1,  2, 1 },                /* Kick '4' Cash */
   { "NINJA ASSAULT"                     , -1, -1, -1, -1, -1, -1,  -1,  7, 1 },                /* Ninja Assault */

   /* Input descriptors */
   { " BIOHAZARD  GUN SURVIVOR2"         , -1, -1, -1, -1, -1, -1,  -1, -1, 1, &gunsur2_inputs }, /* Gun Survivor 2 Biohazard Code: Veronica */
};
