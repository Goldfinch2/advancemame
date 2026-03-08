/*

    Atari Tomcat prototype hardware

    Driver by Mariusz Wojcieszek
    Ported from MAME 0.138 to 0.106

    Notes:
    - game has no sound, while sound hardware was developed, sound program was
      not prepared

    TODO:
    - add proper timing of interrupts and framerate (currently commented out,
      as they cause test mode to hang)
    - vector quality appears to be worse than original game (compared to original
      game videos)
    - verify controls
    - implement game linking (after MAME supports network)
    - current implementation of 68010 <-> tms32010 is a little bit hacky, after
      tms32010 is started by 68010, 68010 is suspended until tms32010 reads command
      and starts executing
    - hook up tms5220 - it is currently not used at all

*/

#include "driver.h"
#include "cpu/m68000/m68000.h"
#include "cpu/tms32010/tms32010.h"
#include "cpu/m6502/m6502.h"
#include "vidhrdw/vector.h"
#include "vidhrdw/avgdvg.h"
#include "machine/timekpr.h"
#include "machine/6532riot.h"
#include "sound/pokey.h"
#include "sound/5220intf.h"
#include "sound/2151intf.h"

static int tomcat_control_num;
static UINT16 *tomcat_shared_ram;
static UINT16 *tomcat_mainram;
static UINT8 *tomcat_nvram;
static int dsp_BIO;
static int dsp_idle;

/* CPU indices: 0 = maincpu (68010), 1 = dsp (TMS32010), 2 = soundcpu (6502) */
#define CPU_MAIN	0
#define CPU_DSP		1
#define CPU_SOUND	2

static WRITE16_HANDLER(tomcat_adcon_w)
{
	tomcat_control_num = data;
}

static READ16_HANDLER(tomcat_adcread_r)
{
	switch( tomcat_control_num )
	{
	case 0: return readinputport(1); /* STICKY */
	case 1: return readinputport(2); /* STICKX */
	default: return 0x7f7f;
	}
}

static READ16_HANDLER(tomcat_inputs_r)
{
	UINT16 result = 0;
	if (~mem_mask & 0xff00)
		result |= (readinputport(0) | (avgdvg_done() ? 1 : 0)) << 8;

	return result;
}

static INTERRUPT_GEN(tomcat_vblank_irq)
{
	cpunum_set_input_line(CPU_MAIN, 1, ASSERT_LINE);
}


static WRITE16_HANDLER(tomcat_led1on_w)
{
	set_led_status(1, 1);
}

static WRITE16_HANDLER(tomcat_led2on_w)
{
	set_led_status(2, 1);
}

static WRITE16_HANDLER(tomcat_led2off_w)
{
	set_led_status(2, 0);
}

static WRITE16_HANDLER(tomcat_led1off_w)
{
	set_led_status(1, 0);
}

static WRITE16_HANDLER(tomcat_lnkmodel_w)
{
	/* Link Mode Low (address strobe) */
	/* Master does not respond to Interrupts */
}

static WRITE16_HANDLER(tomcat_errl_w)
{
	/* Link Error Flag Low (address strobe) */
}

static WRITE16_HANDLER(tomcat_errh_w)
{
	/* Link Error Flag High (address strobe) */
}

static WRITE16_HANDLER(tomcat_ackl_w)
{
	/* Link ACK Flag Low (address strobe) */
}

static WRITE16_HANDLER(tomcat_ackh_w)
{
	/* Link ACK Flag High (address strobe) */
}

static WRITE16_HANDLER(tomcat_lnkmodeh_w)
{
	/* Link Mode high (address strobe) */
}

static WRITE16_HANDLER(tomcat_txbuffl_w)
{
	/* Link Buffer Control (address strobe) */
}

static WRITE16_HANDLER(tomcat_txbuffh_w)
{
	/* Link Buffer Control high (address strobe) */
	/* Turn off TX (Link) Buffer */
}

static WRITE16_HANDLER(tomcat_sndresl_w)
{
	/* Sound Reset Low       (Address Strobe) */
	/* Reset Sound System */
}

static WRITE16_HANDLER(tomcat_sndresh_w)
{
	/* Sound Reset high      (Address Strobe) */
	/* Release reset of sound system */
}

static WRITE16_HANDLER(tomcat_mresl_w)
{
	/* 320 Reset Low         (Address Strobe) */
	/* Reset TMS320 */
	cpunum_set_input_line(CPU_DSP, INPUT_LINE_RESET, ASSERT_LINE);
}

static WRITE16_HANDLER(tomcat_mresh_w)
{
	/* 320 Reset high        (Address Strobe) */
	/* Release reset of TMS320 */
	dsp_BIO = 0;
	cpunum_set_input_line(CPU_DSP, INPUT_LINE_RESET, CLEAR_LINE);
}

static WRITE16_HANDLER(tomcat_irqclr_w)
{
	/* Clear IRQ Latch          (Address Strobe) */
	cpunum_set_input_line(CPU_MAIN, 1, CLEAR_LINE);
}

static READ16_HANDLER(tomcat_inputs2_r)
{
/*
*       D15 LNKFLAG     (Game Link)
*       D14 PC3        "    "
*       D13 PC2        "    "
*       D12 PC0        "    "
*       D11 MAINFLAG    (Sound System)
*       D10 SOUNDFLAG      "    "
*       D9  /IDLE*      (TMS320 System)
*       D8
*/
	return dsp_idle ? 0 : (1 << 9);
}

static READ16_HANDLER(tomcat_320bio_r)
{
	dsp_BIO = 1;
	cpunum_suspend(CPU_MAIN, SUSPEND_REASON_SPIN, 1);
	return 0;
}

static READ16_HANDLER(dsp_BIO_r)
{
	if ( activecpu_get_pc() == 0x0001 )
	{
		if ( dsp_idle == 0 )
		{
			dsp_idle = 1;
			dsp_BIO = 0;
		}
		return !dsp_BIO;
	}
	else if ( activecpu_get_pc() == 0x0003 )
	{
		if ( dsp_BIO == 1 )
		{
			dsp_idle = 0;
			dsp_BIO = 0;
			cpunum_resume(CPU_MAIN, SUSPEND_REASON_SPIN);
			return 0;
		}
		else
		{
			assert(0);
			return 0;
		}
	}
	else
	{
		return !dsp_BIO;
	}
}

static READ16_HANDLER(tomcat_shared_ram_r)
{
	return tomcat_shared_ram[offset];
}

static WRITE16_HANDLER(tomcat_shared_ram_w)
{
	COMBINE_DATA(&tomcat_shared_ram[offset]);
}

static READ16_HANDLER(tomcat_nvram_r)
{
	return tomcat_nvram[offset];
}

static WRITE16_HANDLER(tomcat_nvram_w)
{
	tomcat_nvram[offset] = data & 0xff;
}

/* Combined handler: timekeeper on upper byte (0xff00), nvram on lower byte (0x00ff)
   In MAME 0.138 these are two overlapping mappings with byte masks at 0xffd000 */
static READ16_HANDLER(tomcat_ffd000_r)
{
	UINT16 result = timekeeper_0_msb16_r(offset, mem_mask);
	if (ACCESSING_LSB16)
		result |= tomcat_nvram[offset] & 0xff;
	return result;
}

static WRITE16_HANDLER(tomcat_ffd000_w)
{
	timekeeper_0_msb16_w(offset, data, mem_mask);
	if (ACCESSING_LSB16)
		tomcat_nvram[offset] = data & 0xff;
}

static ADDRESS_MAP_START( tomcat_map, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x00ffff) AM_ROM
	AM_RANGE(0x402000, 0x402001) AM_READ(tomcat_adcread_r) AM_WRITE(tomcat_adcon_w)
	AM_RANGE(0x404000, 0x404001) AM_READ(tomcat_inputs_r) AM_WRITE(avgdvg_go_word_w)
	AM_RANGE(0x406000, 0x406001) AM_WRITE(avgdvg_reset_word_w)
	AM_RANGE(0x408000, 0x408001) AM_READWRITE(tomcat_inputs2_r, watchdog_reset16_w)
	AM_RANGE(0x40a000, 0x40a001) AM_READWRITE(tomcat_320bio_r, tomcat_irqclr_w)
	AM_RANGE(0x40e000, 0x40e001) AM_WRITE(tomcat_led1on_w)
	AM_RANGE(0x40e002, 0x40e003) AM_WRITE(tomcat_led2on_w)
	AM_RANGE(0x40e004, 0x40e005) AM_WRITE(tomcat_mresl_w)
	AM_RANGE(0x40e006, 0x40e007) AM_WRITE(tomcat_sndresl_w)
	AM_RANGE(0x40e008, 0x40e009) AM_WRITE(tomcat_lnkmodel_w)
	AM_RANGE(0x40e00a, 0x40e00b) AM_WRITE(tomcat_errl_w)
	AM_RANGE(0x40e00c, 0x40e00d) AM_WRITE(tomcat_ackl_w)
	AM_RANGE(0x40e00e, 0x40e00f) AM_WRITE(tomcat_txbuffl_w)
	AM_RANGE(0x40e010, 0x40e011) AM_WRITE(tomcat_led1off_w)
	AM_RANGE(0x40e012, 0x40e013) AM_WRITE(tomcat_led2off_w)
	AM_RANGE(0x40e014, 0x40e015) AM_WRITE(tomcat_mresh_w)
	AM_RANGE(0x40e016, 0x40e017) AM_WRITE(tomcat_sndresh_w)
	AM_RANGE(0x40e018, 0x40e019) AM_WRITE(tomcat_lnkmodeh_w)
	AM_RANGE(0x40e01a, 0x40e01b) AM_WRITE(tomcat_errh_w)
	AM_RANGE(0x40e01c, 0x40e01d) AM_WRITE(tomcat_ackh_w)
	AM_RANGE(0x40e01e, 0x40e01f) AM_WRITE(tomcat_txbuffh_w)
	AM_RANGE(0x800000, 0x803fff) AM_RAM AM_BASE((UINT16**)&vectorram) AM_SIZE(&vectorram_size)
	AM_RANGE(0xffa000, 0xffbfff) AM_READWRITE(tomcat_shared_ram_r, tomcat_shared_ram_w)
	AM_RANGE(0xffc000, 0xffcfff) AM_RAM AM_BASE(&tomcat_mainram)
	AM_RANGE(0xffd000, 0xffdfff) AM_READWRITE(tomcat_ffd000_r, tomcat_ffd000_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( dsp_map, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x0000, 0x1fff) AM_RAM AM_BASE(&tomcat_shared_ram)
ADDRESS_MAP_END

static ADDRESS_MAP_START( dsp_io_map, ADDRESS_SPACE_IO, 16 )
	AM_RANGE(TMS32010_BIO, TMS32010_BIO) AM_READ(dsp_BIO_r)
ADDRESS_MAP_END

static WRITE8_HANDLER(soundlatches_w)
{
	switch(offset)
	{
		case 0x00: break; /* XLOAD 0    Write the Sequential ROM counter Low Byte */
		case 0x20: break; /* XLOAD 1    Write the Sequential ROM counter High Byte */
		case 0x40: break; /* SOUNDWR    Write to Sound Interface Latch (read by Main) */
		case 0x60: break; /* unused */
		case 0x80: break; /* XREAD      Read the Sequential ROM (64K bytes) and increment the counter */
		case 0xa0: break; /* unused */
		case 0xc0: break; /* SOUNDREAD  Read the Sound Interface Latch (written by Main) */
	}
}

static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x1fff) AM_RAM
	AM_RANGE(0x2000, 0x2000) AM_WRITE(YM2151_register_port_0_w)
	AM_RANGE(0x2001, 0x2001) AM_READWRITE(YM2151_status_port_0_r, YM2151_data_port_0_w)
	AM_RANGE(0x3000, 0x30df) AM_WRITE(soundlatches_w)
	AM_RANGE(0x30e0, 0x30e0) AM_NOP /* COINRD Inputs: D7 = Coin L, D6 = Coin R, D5 = SOUNDFLAG */
	AM_RANGE(0x5000, 0x507f) AM_RAM	/* 6532 ram */
	AM_RANGE(0x5080, 0x509f) AM_READWRITE(r6532_0_r, r6532_0_w)
	AM_RANGE(0x6000, 0x601f) AM_READWRITE(pokey1_r, pokey1_w)
	AM_RANGE(0x7000, 0x701f) AM_READWRITE(pokey2_r, pokey2_w)
	AM_RANGE(0x8000, 0xffff) AM_NOP /* main sound program rom */
ADDRESS_MAP_END

static INPUT_PORTS_START( tomcat )
	PORT_START	/* IN0 - INPUTS */
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL ) /* avgdvg done */
	PORT_BIT( 0x02, IP_ACTIVE_LOW,  IPT_UNUSED ) /* SPARE */
	PORT_BIT( 0x04, IP_ACTIVE_LOW,  IPT_BUTTON5 ) /* DIAGNOSTIC */
	PORT_SERVICE( 0x08, IP_ACTIVE_LOW )
	PORT_BIT( 0x10, IP_ACTIVE_LOW,  IPT_BUTTON2 ) /* R FIRE */
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_BUTTON1 ) /* L FIRE */
	PORT_BIT( 0x40, IP_ACTIVE_LOW,  IPT_BUTTON4 ) /* R THUMB */
	PORT_BIT( 0x80, IP_ACTIVE_LOW,  IPT_BUTTON3 ) /* L THUMB */

	PORT_START	/* IN1 - STICKY */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Y ) PORT_SENSITIVITY(70) PORT_KEYDELTA(30)

	PORT_START	/* IN2 - STICKX */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_X ) PORT_SENSITIVITY(50) PORT_KEYDELTA(30)
INPUT_PORTS_END

ROM_START( tomcat )
	ROM_REGION( 0x10000, REGION_CPU1, 0)
	ROM_LOAD16_BYTE( "rom1k.bin", 0x00001, 0x8000, CRC(5535a1ff) SHA1(b9807c749a8e6b5ddec3ff494130abda09f0baab) )
	ROM_LOAD16_BYTE( "rom2k.bin", 0x00000, 0x8000, CRC(021a01d2) SHA1(01d99aab54ad57a664e8aaa91296bb879fc6e422) )

	ROM_REGION( 0x100, REGION_USER1, 0 )
	ROM_LOAD( "136021-105.1l",   0x0000, 0x0100, CRC(82fc3eb2) SHA1(184231c7baef598294860a7d2b8a23798c5c7da6) ) /* AVG PROM */
ROM_END

static struct R6532interface tomcat_r6532_intf =
{
/*
    PA0 = /WS   OUTPUT  (TMS-5220 WRITE STROBE)
    PA1 = /RS   OUTPUT  (TMS-5220 READ STROBE)
    PA2 = /READY    INPUT   (TMS-5220 READY FLAG)
    PA3 = FSEL  OUTPUT  Select TMS5220 clock;
                0 = 325 KHz (8 KHz sampling)
                1 = 398 KHz (10 KHz sampling)
    PA4 = /CC1  OUTPUT  Coin Counter 1
    PA5 = /CC2  OUTPUT  Coin Counter 2
    PA6 = /MUSRES   OUTPUT  (Reset the Yamaha)
    PA7 = MAINFLAG  INPUT
*/
	NULL, /* portA_r */
	NULL, /* portB_r */  /*  PB0 - PB7   OUTPUT  Speech Data */
	NULL, /* portA_w */
	NULL, /* portB_w */
	NULL  /* irq_func */ /* connected to IRQ line of 6502 */
};

static MACHINE_START(tomcat)
{
	((UINT16*)tomcat_shared_ram)[0x0000] = 0xf600;
	((UINT16*)tomcat_shared_ram)[0x0001] = 0x0000;
	((UINT16*)tomcat_shared_ram)[0x0002] = 0xf600;
	((UINT16*)tomcat_shared_ram)[0x0003] = 0x0000;

	tomcat_nvram = auto_malloc(0x800);

	r6532_init(0, &tomcat_r6532_intf);
	timekeeper_init(0, TIMEKEEPER_M48T02, NULL);

	state_save_register_global_pointer(tomcat_nvram, 0x800);
	state_save_register_global(tomcat_control_num);
	state_save_register_global(dsp_BIO);
	state_save_register_global(dsp_idle);

	dsp_BIO = 0;

	return 0;
}

static NVRAM_HANDLER(tomcat)
{
	if (read_or_write)
		mame_fwrite(file, tomcat_nvram, 0x800);
	else
		if (file)
			mame_fread(file, tomcat_nvram, 0x800);
		else
			memset(tomcat_nvram, 0x00, 0x800);
}

static MACHINE_DRIVER_START(tomcat)
	MDRV_CPU_ADD(M68010, 12000000 / 2)
	MDRV_CPU_PROGRAM_MAP(tomcat_map, 0)
	MDRV_CPU_VBLANK_INT(tomcat_vblank_irq, 5)

	MDRV_CPU_ADD(TMS32010, 16000000)
	MDRV_CPU_PROGRAM_MAP(dsp_map, 0)
	MDRV_CPU_IO_MAP(dsp_io_map, 0)

	MDRV_CPU_ADD(M6502, 14318180 / 8)
	MDRV_CPU_FLAGS(CPU_DISABLE)
	MDRV_CPU_PROGRAM_MAP(sound_map, 0)

	MDRV_INTERLEAVE(4000)

	MDRV_MACHINE_START(tomcat)

	MDRV_NVRAM_HANDLER(tomcat)

	MDRV_FRAMES_PER_SECOND(40)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_VECTOR | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(400, 300)
	MDRV_VISIBLE_AREA(0, 280, 0, 250)
	MDRV_PALETTE_LENGTH(32768)

	MDRV_PALETTE_INIT(avg_multi)
	MDRV_VIDEO_START(avg_tomcat)
	MDRV_VIDEO_UPDATE(vector)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MDRV_SOUND_ADD(POKEY, 14318180 / 8)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.20)

	MDRV_SOUND_ADD(POKEY, 14318180 / 8)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.20)

	MDRV_SOUND_ADD(TMS5220, 325000)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 0.50)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 0.50)

	MDRV_SOUND_ADD(YM2151, 14318180 / 4)
	MDRV_SOUND_ROUTE(0, "lspeaker", 0.60)
	MDRV_SOUND_ROUTE(1, "rspeaker", 0.60)
MACHINE_DRIVER_END

GAME( 1985, tomcat, 0, tomcat, tomcat, 0, ROT0, "Atari", "TomCat (prototype)", GAME_NO_SOUND )
