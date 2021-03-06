/***************************************************************************

  a7800.c

  Driver file to handle emulation of the Atari 7800.

  Dan Boris

	2002/05/13 kubecj	added more banks for bankswitching
							added PAL machine description
							changed clock to be precise

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"
#include "sound/tiaintf.h"
#include "devices/cartslot.h"
#include "machine/6532riot.h"
#include "includes/a7800.h"

static ADDRESS_MAP_START(a7800_mem, ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x001F) AM_READWRITE(a7800_TIA_r, a7800_TIA_w)
	AM_RANGE(0x0020, 0x003F) AM_READWRITE(a7800_MARIA_r, a7800_MARIA_w)
	AM_RANGE(0x0040, 0x00FF) AM_READWRITE(MRA8_BANK5, a7800_RAM0_w)		/* RAM0 */
	AM_RANGE(0x0100, 0x011F) AM_READWRITE(a7800_TIA_r, a7800_TIA_w)
	AM_RANGE(0x0120, 0x013F) AM_READWRITE(a7800_MARIA_r, a7800_MARIA_w)
	AM_RANGE(0x0140, 0x01FF) AM_READWRITE(MRA8_BANK6, MWA8_BANK6)		/* RAM1 */
	AM_RANGE(0x0200, 0x021F) AM_READWRITE(a7800_TIA_r, a7800_TIA_w)
	AM_RANGE(0x0220, 0x023F) AM_READWRITE(a7800_MARIA_r, a7800_MARIA_w)
	AM_RANGE(0x0280, 0x029F) AM_READWRITE(r6532_0_r, r6532_0_w)
	AM_RANGE(0x0300, 0x031F) AM_READWRITE(a7800_TIA_r, a7800_TIA_w)
	AM_RANGE(0x0320, 0x033F) AM_READWRITE(a7800_MARIA_r, a7800_MARIA_w)
	AM_RANGE(0x0480, 0x04FF) AM_RAM										/* RIOT RAM */
	AM_RANGE(0x1800, 0x27FF) AM_RAM
	AM_RANGE(0x2800, 0x2FFF) AM_READWRITE(MRA8_BANK7, MWA8_BANK7)		/* MAINRAM */
	AM_RANGE(0x3000, 0x37FF) AM_READWRITE(MRA8_BANK7, MWA8_BANK7)		/* MAINRAM */
	AM_RANGE(0x3800, 0x3FFF) AM_READWRITE(MRA8_BANK7, MWA8_BANK7)		/* MAINRAM */

	AM_RANGE(0x4000, 0x7FFF) AM_READ(MRA8_BANK1)						/* f18 hornet */
	AM_RANGE(0x8000, 0x9FFF) AM_READ(MRA8_BANK2)						/* sc */
	AM_RANGE(0xA000, 0xBFFF) AM_READ(MRA8_BANK3)						/* sc + ac */
	AM_RANGE(0xC000, 0xDFFF) AM_READ(MRA8_BANK4)						/* ac */
	AM_RANGE(0xE000, 0xFFFF) AM_READ(MRA8_ROM)
	AM_RANGE(0x4000, 0xFFFF) AM_WRITE(a7800_cart_w)
ADDRESS_MAP_END



INPUT_PORTS_START( a7800 )
	PORT_START      /* IN0 */
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP) PORT_PLAYER(2)
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN) PORT_PLAYER(2)
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT) PORT_PLAYER(2)
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT) PORT_PLAYER(2)
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP) PORT_PLAYER(1)
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN) PORT_PLAYER(1)
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT) PORT_PLAYER(1)
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT) PORT_PLAYER(1)

	PORT_START      /* IN1 */
	PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(2)
	PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_PLAYER(1)
	PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(2)
	PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_PLAYER(1)
	PORT_BIT ( 0xF0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT (0x7F, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT (0x80, IP_ACTIVE_HIGH, IPT_VBLANK)

	PORT_START      /* IN3 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("Reset") PORT_CODE(KEYCODE_2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START) PORT_NAME("Select")
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNKNOWN) PORT_NAME(DEF_STR(Pause)) PORT_CODE(KEYCODE_CLOSEBRACE)
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_DIPNAME( 0x40, 0x00, "Left Diff. Switch" ) PORT_CODE(KEYCODE_3) PORT_TOGGLE
	PORT_DIPSETTING(    0x40, "A" )
	PORT_DIPSETTING(    0x00, "B" )
	PORT_DIPNAME( 0x80, 0x00, "Right Diff. Switch" ) PORT_CODE(KEYCODE_4) PORT_TOGGLE
	PORT_DIPSETTING(    0x80, "A" )
	PORT_DIPSETTING(    0x00, "B" )
INPUT_PORTS_END

#define NTSC_GREY    \
	0x00,0x00,0x00, 0x1c,0x1c,0x1c, 0x39,0x39,0x39, 0x59,0x59,0x59,	\
	0x79,0x79,0x79, 0x92,0x92,0x92, 0xab,0xab,0xab, 0xbc,0xbc,0xbc,	\
	0xcd,0xcd,0xcd, 0xd9,0xd9,0xd9, 0xe6,0xe6,0xe6, 0xec,0xec,0xec,	\
	0xf2,0xf2,0xf2, 0xf8,0xf8,0xf8, 0xff,0xff,0xff, 0xff,0xff,0xff

#define NTSC_GOLD \
	0x39,0x17,0x01, 0x5e,0x23,0x04, 0x83,0x30,0x08, 0xa5,0x47,0x16, \
	0xc8,0x5f,0x24, 0xe3,0x78,0x20, 0xff,0x91,0x1d, 0xff,0xab,0x1d, \
	0xff,0xc5,0x1d, 0xff,0xce,0x34, 0xff,0xd8,0x4c, 0xff,0xe6,0x51, \
	0xff,0xf4,0x56, 0xff,0xf9,0x77, 0xff,0xff,0x98, 0xff,0xff,0x98

#define NTSC_ORANGE \
	0x45,0x19,0x04, 0x72,0x1e,0x11, 0x9f,0x24,0x1e, 0xb3,0x3a,0x20, \
	0xc8,0x51,0x22, 0xe3,0x69,0x20, 0xff,0x81,0x1e, 0xff,0x8c,0x25, \
	0xff,0x98,0x2c, 0xff,0xae,0x38, 0xff,0xc5,0x45, 0xff,0xc5,0x59, \
	0xff,0xc6,0x6d, 0xff,0xd5,0x87, 0xff,0xe4,0xa1, 0xff,0xe4,0xa1

#define NTSC_RED_ORANGE \
	0x4a,0x17,0x04, 0x7e,0x1a,0x0d, 0xb2,0x1d,0x17, 0xc8,0x21,0x19, \
	0xdf,0x25,0x1c, 0xec,0x3b,0x38, 0xfa,0x52,0x55, 0xfc,0x61,0x61, \
	0xff,0x70,0x6e, 0xff,0x7f,0x7e, 0xff,0x8f,0x8f, 0xff,0x9d,0x9e, \
	0xff,0xab,0xad, 0xff,0xb9,0xbd, 0xff,0xc7,0xce, 0xff,0xc7,0xce

#define NTSC_PINK \
	0x05,0x05,0x68, 0x3b,0x13,0x6d, 0x71,0x22,0x72, 0x8b,0x2a,0x8c, \
	0xa5,0x32,0xa6, 0xb9,0x38,0xba, 0xcd,0x3e,0xcf, 0xdb,0x47,0xdd, \
	0xea,0x51,0xeb, 0xf4,0x5f,0xf5, 0xfe,0x6d,0xff, 0xfe,0x7a,0xfd, \
	0xff,0x87,0xfb, 0xff,0x95,0xfd, 0xff,0xa4,0xff, 0xff,0xa4,0xff

#define NTSC_PURPLE \
	0x28,0x04,0x79, 0x40,0x09,0x84, 0x59,0x0f,0x90, 0x70,0x24,0x9d, \
	0x88,0x39,0xaa, 0xa4,0x41,0xc3, 0xc0,0x4a,0xdc, 0xd0,0x54,0xed, \
	0xe0,0x5e,0xff, 0xe9,0x6d,0xff, 0xf2,0x7c,0xff, 0xf8,0x8a,0xff, \
	0xff,0x98,0xff, 0xfe,0xa1,0xff, 0xfe,0xab,0xff, 0xfe,0xab,0xff

#define NTSC_PURPLE_BLUE \
	0x35,0x08,0x8a, 0x42,0x0a,0xad, 0x50,0x0c,0xd0, 0x64,0x28,0xd0, \
	0x79,0x45,0xd0, 0x8d,0x4b,0xd4, 0xa2,0x51,0xd9, 0xb0,0x58,0xec, \
	0xbe,0x60,0xff, 0xc5,0x6b,0xff, 0xcc,0x77,0xff, 0xd1,0x83,0xff, \
	0xd7,0x90,0xff, 0xdb,0x9d,0xff, 0xdf,0xaa,0xff, 0xdf,0xaa,0xff

#define NTSC_BLUE1 \
	0x05,0x1e,0x81, 0x06,0x26,0xa5, 0x08,0x2f,0xca, 0x26,0x3d,0xd4, \
	0x44,0x4c,0xde, 0x4f,0x5a,0xee, 0x5a,0x68,0xff, 0x65,0x75,0xff, \
	0x71,0x83,0xff, 0x80,0x91,0xff, 0x90,0xa0,0xff, 0x97,0xa9,0xff, \
	0x9f,0xb2,0xff, 0xaf,0xbe,0xff, 0xc0,0xcb,0xff, 0xc0,0xcb,0xff

#define NTSC_BLUE2 \
	0x0c,0x04,0x8b, 0x22,0x18,0xa0, 0x38,0x2d,0xb5, 0x48,0x3e,0xc7, \
	0x58,0x4f,0xda, 0x61,0x59,0xec, 0x6b,0x64,0xff, 0x7a,0x74,0xff, \
	0x8a,0x84,0xff, 0x91,0x8e,0xff, 0x99,0x98,0xff, 0xa5,0xa3,0xff, \
	0xb1,0xae,0xff, 0xb8,0xb8,0xff, 0xc0,0xc2,0xff, 0xc0,0xc2,0xff

#define NTSC_LIGHT_BLUE \
	0x1d,0x29,0x5a, 0x1d,0x38,0x76, 0x1d,0x48,0x92, 0x1c,0x5c,0xac, \
	0x1c,0x71,0xc6, 0x32,0x86,0xcf, 0x48,0x9b,0xd9, 0x4e,0xa8,0xec, \
	0x55,0xb6,0xff, 0x70,0xc7,0xff, 0x8c,0xd8,0xff, 0x93,0xdb,0xff, \
	0x9b,0xdf,0xff, 0xaf,0xe4,0xff, 0xc3,0xe9,0xff, 0xc3,0xe9,0xff

#define NTSC_TURQUOISE \
	0x2f,0x43,0x02, 0x39,0x52,0x02, 0x44,0x61,0x03, 0x41,0x7a,0x12, \
	0x3e,0x94,0x21, 0x4a,0x9f,0x2e, 0x57,0xab,0x3b, 0x5c,0xbd,0x55, \
	0x61,0xd0,0x70, 0x69,0xe2,0x7a, 0x72,0xf5,0x84, 0x7c,0xfa,0x8d, \
	0x87,0xff,0x97, 0x9a,0xff,0xa6, 0xad,0xff,0xb6, 0xad,0xff,0xb6

#define NTSC_GREEN_BLUE	\
	0x0a,0x41,0x08, 0x0d,0x54,0x0a, 0x10,0x68,0x0d, 0x13,0x7d,0x0f, \
	0x16,0x92,0x12, 0x19,0xa5,0x14, 0x1c,0xb9,0x17, 0x1e,0xc9,0x19, \
	0x21,0xd9,0x1b, 0x47,0xe4,0x2d, 0x6e,0xf0,0x40, 0x78,0xf7,0x4d, \
	0x83,0xff,0x5b, 0x9a,0xff,0x7a, 0xb2,0xff,0x9a, 0xb2,0xff,0x9a

#define NTSC_GREEN \
	0x04,0x41,0x0b, 0x05,0x53,0x0e, 0x06,0x66,0x11, 0x07,0x77,0x14, \
	0x08,0x88,0x17, 0x09,0x9b,0x1a, 0x0b,0xaf,0x1d, 0x48,0xc4,0x1f, \
	0x86,0xd9,0x22, 0x8f,0xe9,0x24, 0x99,0xf9,0x27, 0xa8,0xfc,0x41, \
	0xb7,0xff,0x5b, 0xc9,0xff,0x6e, 0xdc,0xff,0x81, 0xdc,0xff,0x81

#define NTSC_YELLOW_GREEN \
	0x02,0x35,0x0f, 0x07,0x3f,0x15, 0x0c,0x4a,0x1c, 0x2d,0x5f,0x1e, \
	0x4f,0x74,0x20, 0x59,0x83,0x24, 0x64,0x92,0x28, 0x82,0xa1,0x2e, \
	0xa1,0xb0,0x34, 0xa9,0xc1,0x3a, 0xb2,0xd2,0x41, 0xc4,0xd9,0x45, \
	0xd6,0xe1,0x49, 0xe4,0xf0,0x4e, 0xf2,0xff,0x53, 0xf2,0xff,0x53

#define NTSC_ORANGE_GREEN \
	0x26,0x30,0x01, 0x24,0x38,0x03, 0x23,0x40,0x05, 0x51,0x54,0x1b, \
	0x80,0x69,0x31, 0x97,0x81,0x35, 0xaf,0x99,0x3a, 0xc2,0xa7,0x3e, \
	0xd5,0xb5,0x43, 0xdb,0xc0,0x3d, 0xe1,0xcb,0x38, 0xe2,0xd8,0x36, \
	0xe3,0xe5,0x34, 0xef,0xf2,0x58, 0xfb,0xff,0x7d, 0xfb,0xff,0x7d

#define NTSC_LIGHT_ORANGE \
	0x40,0x1a,0x02, 0x58,0x1f,0x05, 0x70,0x24,0x08, 0x8d,0x3a,0x13, \
	0xab,0x51,0x1f, 0xb5,0x64,0x27, 0xbf,0x77,0x30, 0xd0,0x85,0x3a, \
	0xe1,0x93,0x44, 0xed,0xa0,0x4e, 0xf9,0xad,0x58, 0xfc,0xb7,0x5c, \
	0xff,0xc1,0x60, 0xff,0xc6,0x71, 0xff,0xcb,0x83, 0xff,0xcb,0x83

static UINT8 a7800_palette[256*3] =
{
	NTSC_GREY,
	NTSC_GOLD,
	NTSC_ORANGE,
	NTSC_RED_ORANGE,
	NTSC_PINK,
	NTSC_PURPLE,
	NTSC_PURPLE_BLUE,
	NTSC_BLUE1,
	NTSC_BLUE2,
	NTSC_LIGHT_BLUE,
	NTSC_TURQUOISE,
	NTSC_GREEN_BLUE,
	NTSC_GREEN,
	NTSC_YELLOW_GREEN,
	NTSC_ORANGE_GREEN,
	NTSC_LIGHT_ORANGE
};

static UINT8 a7800p_palette[256*3] =
{
	NTSC_GREY,
	NTSC_ORANGE_GREEN,
	NTSC_GOLD,
	NTSC_ORANGE,
	NTSC_RED_ORANGE,
	NTSC_PINK,
	NTSC_PURPLE,
	NTSC_PURPLE_BLUE,
	NTSC_BLUE1,
	NTSC_BLUE1,
	NTSC_BLUE2,
	NTSC_LIGHT_BLUE,
	NTSC_TURQUOISE,
	NTSC_GREEN_BLUE,
	NTSC_GREEN,
	NTSC_YELLOW_GREEN
};

static unsigned short a7800_colortable[] =
{
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,
    0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
    0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,
    0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5e,0x5f,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x6f,
    0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,0x7b,0x7c,0x7d,0x7e,0x7f,
    0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x8b,0x8c,0x8d,0x8e,0x8f,
    0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x9f,
    0xa0,0xa1,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,
    0xb0,0xb1,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xbb,0xbc,0xbd,0xbe,0xbf,
    0xc0,0xc1,0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,
    0xd0,0xd1,0xd2,0xd3,0xd4,0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xdb,0xdc,0xdd,0xde,0xdf,
    0xe0,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xec,0xed,0xee,0xef,
    0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa,0xfb,0xfc,0xfd,0xfe,0xff
};


/* Initialise the palette */
static PALETTE_INIT(a7800)
{

	palette_set_colors(0, a7800_palette, sizeof(a7800_palette) / 3);
    memcpy(colortable,a7800_colortable,sizeof(a7800_colortable));

}


static PALETTE_INIT(a7800p)
{
	palette_set_colors(0, a7800p_palette, sizeof(a7800p_palette) / 3);
    memcpy(colortable,a7800_colortable,sizeof(a7800_colortable));
}


#define CLK_PAL 1773447
#define CLK_NTSC 1789772



static MACHINE_DRIVER_START( a7800_ntsc )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M6502, CLK_NTSC)	/* 1.79Mhz (note: The clock switches to 1.19Mhz
												 * when the TIA or RIOT are accessed) */
	MDRV_CPU_PROGRAM_MAP(a7800_mem, 0)
	MDRV_CPU_VBLANK_INT(a7800_interrupt,262)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(1)

	MDRV_MACHINE_RESET( a7800 )

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(640,262)
	MDRV_VISIBLE_AREA(0,319,25,45+204)
	MDRV_PALETTE_LENGTH(sizeof(a7800_palette) / sizeof(a7800_palette[0]) / 3)
	MDRV_COLORTABLE_LENGTH(sizeof(a7800_colortable) / sizeof(a7800_colortable[0]))
	MDRV_PALETTE_INIT(a7800)

	MDRV_VIDEO_START(a7800)
	MDRV_VIDEO_UPDATE(a7800)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD(TIA, 31400)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
	MDRV_SOUND_ADD_TAG("pokey", POKEY, CLK_NTSC)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
MACHINE_DRIVER_END



static MACHINE_DRIVER_START( a7800_pal )
	MDRV_IMPORT_FROM( a7800_ntsc )

	/* basic machine hardware */
	MDRV_CPU_REPLACE("main", M6502, CLK_PAL)	/* 1.79Mhz (note: The clock switches to 1.19Mhz
												 * when the TIA or RIOT are accessed) */
	MDRV_CPU_VBLANK_INT(a7800_interrupt,312)
	MDRV_FRAMES_PER_SECOND(50)
	MDRV_SCREEN_SIZE(640,312)
	MDRV_VISIBLE_AREA(0,319,50,50+225)
	MDRV_PALETTE_INIT( a7800p )

	/* sound hardware */
	MDRV_SOUND_REPLACE("pokey", POKEY, CLK_PAL)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)
MACHINE_DRIVER_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START (a7800)
    ROM_REGION(0x30000,REGION_CPU1,0)
    ROM_LOAD ("7800.rom", 0xf000, 0x1000, CRC(5d13730c) SHA1(d9d134bb6b36907c615a594cc7688f7bfcef5b43))
/*      ROM_LOAD ("7800a.rom", 0xc000, 0x4000, CRC(649913e5)) */

ROM_END

ROM_START (a7800p)
    ROM_REGION(0x30000,REGION_CPU1,0)
    //ROM_LOAD ("7800pal.rom", 0xF000, 0x1000, CRC(d5b61170 ))
    ROM_LOAD ("7800pal.rom", 0xc000, 0x4000, CRC(d5b61170) SHA1(5a140136a16d1d83e4ff32a19409ca376a8df874))
ROM_END

static void a7800_ntsc_cartslot_getinfo(const device_class *devclass, UINT32 state, union devinfo *info)
{
	/* cartslot */
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_COUNT:							info->i = 1; break;
		case DEVINFO_INT_MUST_BE_LOADED:				info->i = 1; break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_PTR_LOAD:							info->load = device_load_a7800_cart; break;
		case DEVINFO_PTR_PARTIAL_HASH:					info->partialhash = a7800_partialhash; break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case DEVINFO_STR_FILE_EXTENSIONS:				strcpy(info->s = device_temp_str(), "a78"); break;

		default:										cartslot_device_getinfo(devclass, state, info); break;
	}
}

SYSTEM_CONFIG_START(a7800_ntsc)
	CONFIG_DEVICE(a7800_ntsc_cartslot_getinfo)
SYSTEM_CONFIG_END

static void a7800_pal_cartslot_getinfo(const device_class *devclass, UINT32 state, union devinfo *info)
{
	/* cartslot */
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_COUNT:							info->i = 1; break;
		case DEVINFO_INT_MUST_BE_LOADED:				info->i = 1; break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_PTR_LOAD:							info->load = device_load_a7800_cart; break;
		case DEVINFO_PTR_PARTIAL_HASH:					info->partialhash = a7800_partialhash; break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case DEVINFO_STR_FILE_EXTENSIONS:				strcpy(info->s = device_temp_str(), "a78"); break;

		default:										cartslot_device_getinfo(devclass, state, info); break;
	}
}

SYSTEM_CONFIG_START(a7800_pal)
	CONFIG_DEVICE(a7800_pal_cartslot_getinfo)
SYSTEM_CONFIG_END

/***************************************************************************

  Game driver(s)

***************************************************************************/

/*    YEAR  NAME      PARENT    COMPAT	MACHINE		INPUT     INIT			CONFIG		COMPANY   FULLNAME */
CONS( 1986, a7800,    0,        0,		a7800_ntsc,	a7800,    a7800_ntsc,	a7800_ntsc,	"Atari",  "Atari 7800 (NTSC)" , 0)
CONS( 1986, a7800p,   a7800,    0,		a7800_pal,	a7800,    a7800_pal,	a7800_pal,	"Atari",  "Atari 7800 (PAL)" , 0)
