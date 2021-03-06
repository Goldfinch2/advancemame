/*******************************************************************************

PMD-85 driver by Krzysztof Strzecha

What's new:
-----------

27.06.2004	Mato crash fixed.
21.05.2004	V.24 / Tape switch added. V.24 is not emulated.
25.04.2004	PMD-85.1 tape emulation with support for .pmd format files added.
19.04.2004	Verified PMD-85.1 and PMD-85.2 monitor roms and replaced with
		unmodified ones.
		Memory system cleanups.
03.04.2004	PMD-85.3 and Mato (PMD-85.2 clone) drivers.
		Preliminary and not working tape support.
		Reset key fixed. PMD-85.1 fixed.
15.03.2004	Added drivers for: PMD-85.2, PMD-85.2A, PMD-85.2B and Didaktik
		Alfa (PMD-85.1 clone). Keyboard finished. Natural keyboard added.
		Memory system rewritten. I/O system rewritten. Support for Basic
		ROM module added. Video emulation rewritten.
30.11.2002	Memory mapping improved.
06.07.2002	Preliminary driver.

Notes on emulation status and to do list:
-----------------------------------------

1. V.24.
2. Tape emulation for other machines than PMD-85.1.
3. Flash video attribute.
4. External interfaces connectors (K2-K5).
5. Speaker.
6. Verify PMD-85.2A, PMD-85.3, Didaktik Alfa and Mato monitor roms.
7. Verify all Basic roms.
8. 8251 in Didaktik Alfa.
9. Colors (if any).
10. PMD-85, Didaktik Alfa 2 and Didaktik Beta (ROMs and documentation needed).
11. FDD interface (ROMs and disk images needed).
12. "Duch & Pampuch" Mato game displays scores with incorrect characters.

PMD-85 technical information
============================

Memory map:
-----------

	PMD-85.1, PMD-85.2
	------------------

	start-up map (cleared by the first I/O write operation done by the CPU):
	0000-0fff ROM mirror #1
	1000-1fff not mapped
	2000-2fff ROM mirror #2
	3000-3fff not mapped
	4000-7fff Video RAM mirror #1
	8000-8fff ROM
	9000-9fff not mapped
	a000-afff ROM mirror #3
	b000-bfff not mapped
	c000-ffff Video RAM

	normal map:
	0000-7fff RAM
	8000-8fff ROM
	9000-9fff not mapped
	a000-afff ROM mirror #1
	b000-bfff not mapped
	c000-ffff Video RAM

	Didaktik Alfa (PMD-85.1 clone)
	------------------------------

	start-up map (cleared by the first I/O write operation done by the CPU):
	0000-0fff ROM mirror
	1000-33ff BASIC mirror
	3400-3fff not mapped
	4000-7fff Video RAM mirror
	8000-8fff ROM
	9000-b3ff BASIC
	b400-bfff not mapped
	c000-ffff Video RAM

	normal map:
	0000-7fff RAM
	8000-8fff ROM
	9000-b3ff BASIC
	b400-bfff not mapped
	c000-ffff Video RAM

	PMD-85.2A, PMD-85.2B
	--------------------

	start-up map (cleared by the first I/O write operation done by the CPU):
	0000-0fff ROM mirror #1
	1000-1fff RAM #2 mirror
	2000-2fff ROM mirror #2
	3000-3fff RAM #3 mirror
	4000-7fff Video RAM mirror #1
	8000-8fff ROM
	9000-9fff RAM #2
	a000-afff ROM mirror #3
	b000-bfff RAM #3
	c000-ffff Video RAM

	normal map:
	0000-7fff RAM #1
	8000-8fff ROM
	9000-9fff RAM #2
	a000-afff ROM mirror #1
	b000-bfff RAM #3
	c000-ffff Video RAM

	PMD-85.3
	--------

	start-up map (cleared by the first I/O write operation done by the CPU):
	0000-1fff ROM mirror #1 read, RAM write
	2000-3fff ROM mirror #2 read, RAM write
	4000-5fff ROM mirror #3 read, RAM write
	6000-7fff ROM mirror #4 read, RAM write
	8000-9fff ROM mirror #5 read, RAM write
	a000-bfff ROM mirror #6 read, RAM write
	c000-dfff ROM mirror #7 read, Video RAM #1 write
	e000-ffff ROM, Video RAM #2 write

	normal map:
	0000-bfff RAM
	c000-dfff Video RAM #1
	e000-ffff Video RAM #2 / ROM read, Video RAM #2 write

	Mato
	----

	start-up map (cleared by the first I/O write operation done by the CPU):
	0000-3fff ROM mirror #1
	4000-7fff Video RAM mirror #1
	8000-bfff ROM
	c000-ffff Video RAM

	normal map:
	0000-7fff RAM
	8000-bfff ROM
	c000-ffff Video RAM

I/O ports
---------

	I/O board
	---------
	1xxx11aa	external interfaces connector (K2)
					
	0xxx11aa	I/O board interfaces
		000111aa	8251 (casette recorder, V24)
		010011aa	8255 (GPIO/0, GPIO/1)
		010111aa	8253
		011111aa	8255 (IMS-2)
	I/O board is not supported by Mato.

	Motherboard
	-----------
	1xxx01aa	8255 (keyboard, speaker, LEDs)
			PMD-85.3 memory banking
			Mato cassette recorder

	ROM Module
	----------
	1xxx10aa	8255 (ROM reading)
	ROM module is not supported by Didaktik Alfa and Mato.


*******************************************************************************/

#include "driver.h"
#include "inputx.h"
#include "cpu/i8085/i8085.h"
#include "vidhrdw/generic.h"
#include "devices/cassette.h"
#include "includes/pmd85.h"
#include "formats/pmd_pmd.h"

/* I/O ports */

ADDRESS_MAP_START( pmd85_readport , ADDRESS_SPACE_IO, 8)
	AM_RANGE( 0x00, 0xff) AM_READ( pmd85_io_r )
ADDRESS_MAP_END

ADDRESS_MAP_START( pmd85_writeport , ADDRESS_SPACE_IO, 8)
	AM_RANGE( 0x00, 0xff) AM_WRITE( pmd85_io_w )
ADDRESS_MAP_END

ADDRESS_MAP_START( mato_readport , ADDRESS_SPACE_IO, 8)
	AM_RANGE( 0x00, 0xff) AM_READ( mato_io_r )
ADDRESS_MAP_END

ADDRESS_MAP_START( mato_writeport , ADDRESS_SPACE_IO, 8)
	AM_RANGE( 0x00, 0xff) AM_WRITE( mato_io_w )
ADDRESS_MAP_END

/* memory w/r functions */

ADDRESS_MAP_START( pmd85_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x0fff) AM_READWRITE(MRA8_BANK1, MWA8_BANK1)
	AM_RANGE(0x1000, 0x1fff) AM_READWRITE(MRA8_BANK2, MWA8_BANK2)
	AM_RANGE(0x2000, 0x2fff) AM_READWRITE(MRA8_BANK3, MWA8_BANK3)
	AM_RANGE(0x3000, 0x3fff) AM_READWRITE(MRA8_BANK4, MWA8_BANK4)
	AM_RANGE(0x4000, 0x7fff) AM_READWRITE(MRA8_BANK5, MWA8_BANK5)
	AM_RANGE(0x8000, 0x8fff) AM_READWRITE(MRA8_BANK6, MWA8_ROM)
	AM_RANGE(0x9000, 0x9fff) AM_READWRITE(MRA8_NOP,   MWA8_NOP)
	AM_RANGE(0xa000, 0xafff) AM_READWRITE(MRA8_BANK7, MWA8_ROM)
	AM_RANGE(0xb000, 0xbfff) AM_READWRITE(MRA8_NOP,   MWA8_NOP)
	AM_RANGE(0xc000, 0xffff) AM_READWRITE(MRA8_BANK8, MWA8_BANK8)
ADDRESS_MAP_END

ADDRESS_MAP_START( pmd852a_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x0fff) AM_READWRITE(MRA8_BANK1, MWA8_BANK1)
	AM_RANGE(0x1000, 0x1fff) AM_READWRITE(MRA8_BANK2, MWA8_BANK2)
	AM_RANGE(0x2000, 0x2fff) AM_READWRITE(MRA8_BANK3, MWA8_BANK3)
	AM_RANGE(0x3000, 0x3fff) AM_READWRITE(MRA8_BANK4, MWA8_BANK4)
	AM_RANGE(0x4000, 0x7fff) AM_READWRITE(MRA8_BANK5, MWA8_BANK5)
	AM_RANGE(0x8000, 0x8fff) AM_READWRITE(MRA8_BANK6, MWA8_ROM)
	AM_RANGE(0x9000, 0x9fff) AM_READWRITE(MRA8_BANK7, MWA8_BANK7)
	AM_RANGE(0xa000, 0xafff) AM_READWRITE(MRA8_BANK8, MWA8_ROM)
	AM_RANGE(0xb000, 0xbfff) AM_READWRITE(MRA8_BANK9, MWA8_BANK9)
	AM_RANGE(0xc000, 0xffff) AM_READWRITE(MRA8_BANK10, MWA8_BANK10)
ADDRESS_MAP_END

ADDRESS_MAP_START( pmd853_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x1fff) AM_READWRITE(MRA8_BANK1, MWA8_BANK9)
	AM_RANGE(0x2000, 0x3fff) AM_READWRITE(MRA8_BANK2, MWA8_BANK10)
	AM_RANGE(0x4000, 0x5fff) AM_READWRITE(MRA8_BANK3, MWA8_BANK11)
	AM_RANGE(0x6000, 0x7fff) AM_READWRITE(MRA8_BANK4, MWA8_BANK12)
	AM_RANGE(0x8000, 0x9fff) AM_READWRITE(MRA8_BANK5, MWA8_BANK13)
	AM_RANGE(0xa000, 0xbfff) AM_READWRITE(MRA8_BANK6, MWA8_BANK14)
	AM_RANGE(0xc000, 0xdfff) AM_READWRITE(MRA8_BANK7, MWA8_BANK15)
	AM_RANGE(0xe000, 0xffff) AM_READWRITE(MRA8_BANK8, MWA8_BANK16)
ADDRESS_MAP_END

ADDRESS_MAP_START( alfa_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x0fff) AM_READWRITE(MRA8_BANK1, MWA8_BANK1)
	AM_RANGE(0x1000, 0x33ff) AM_READWRITE(MRA8_BANK2, MWA8_BANK2)
	AM_RANGE(0x3400, 0x3fff) AM_READWRITE(MRA8_BANK3, MWA8_BANK3)
	AM_RANGE(0x4000, 0x7fff) AM_READWRITE(MRA8_BANK4, MWA8_BANK4)
	AM_RANGE(0x8000, 0x8fff) AM_READWRITE(MRA8_BANK5, MWA8_ROM)
	AM_RANGE(0x9000, 0xb3ff) AM_READWRITE(MRA8_BANK6, MWA8_ROM)
	AM_RANGE(0xb400, 0xbfff) AM_READWRITE(MRA8_NOP,   MWA8_NOP)
	AM_RANGE(0xc000, 0xffff) AM_READWRITE(MRA8_BANK7, MWA8_BANK7)
ADDRESS_MAP_END

ADDRESS_MAP_START( mato_mem , ADDRESS_SPACE_PROGRAM, 8)
	AM_RANGE(0x0000, 0x3fff) AM_READWRITE(MRA8_BANK1, MWA8_BANK1)
	AM_RANGE(0x4000, 0x7fff) AM_READWRITE(MRA8_BANK2, MWA8_BANK2)
	AM_RANGE(0x8000, 0xbfff) AM_READWRITE(MRA8_BANK3, MWA8_ROM)
	AM_RANGE(0xc000, 0xffff) AM_READWRITE(MRA8_BANK4, MWA8_BANK4)
ADDRESS_MAP_END

/* keyboard input */

#define PMD85_KEYBOARD_MAIN											\
	PORT_START /* port 0x00 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K0") PORT_CODE(KEYCODE_ESC) PORT_CHAR(UCHAR_MAMEKEY(F1))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1 !") PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q) PORT_CHAR('q')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A) PORT_CHAR('a')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x01 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K1") PORT_CODE(KEYCODE_F1) PORT_CHAR(UCHAR_MAMEKEY(F2))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2 \"") PORT_CODE(KEYCODE_2) PORT_CHAR('1') PORT_CHAR('\"')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W) PORT_CHAR('w')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S) PORT_CHAR('s')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Z) PORT_CHAR('z')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x02 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K2") PORT_CODE(KEYCODE_F2) PORT_CHAR(UCHAR_MAMEKEY(F3))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3 #") PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E) PORT_CHAR('e')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D) PORT_CHAR('d')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X) PORT_CHAR('x')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x03 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K3") PORT_CODE(KEYCODE_F3) PORT_CHAR(UCHAR_MAMEKEY(F4))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4 $") PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR('$')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R) PORT_CHAR('r')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F) PORT_CHAR('f')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C) PORT_CHAR('c')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x04 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K4") PORT_CODE(KEYCODE_F4) PORT_CHAR(UCHAR_MAMEKEY(F5))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5 %") PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T) PORT_CHAR('t')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G) PORT_CHAR('g')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V) PORT_CHAR('v')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x05 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K5") PORT_CODE(KEYCODE_F5) PORT_CHAR(UCHAR_MAMEKEY(F6))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6 &") PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('&')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Y) PORT_CHAR('y')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H) PORT_CHAR('h')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B) PORT_CHAR('b')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x06 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K6") PORT_CODE(KEYCODE_F6) PORT_CHAR(UCHAR_MAMEKEY(F7))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7 \'") PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('\'')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U) PORT_CHAR('u')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J) PORT_CHAR('j')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N) PORT_CHAR('n')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x07 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K7") PORT_CODE(KEYCODE_F7) PORT_CHAR(UCHAR_MAMEKEY(F8))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8 (") PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('(')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I) PORT_CHAR('i')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K) PORT_CHAR('k')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M) PORT_CHAR('m')			\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x08 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K8") PORT_CODE(KEYCODE_F8) PORT_CHAR(UCHAR_MAMEKEY(F9))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9 )") PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR(')')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O) PORT_CHAR('0')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L) PORT_CHAR('l')			\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", <") PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR('<')		\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x09 */										\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K9") PORT_CODE(KEYCODE_F9) PORT_CHAR(UCHAR_MAMEKEY(F10))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0 -") PORT_CODE(KEYCODE_0) PORT_CHAR('0') PORT_CHAR('-')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P) PORT_CHAR('p')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("; +") PORT_CODE(KEYCODE_COLON) PORT_CHAR(';') PORT_CHAR('+')		\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(". >") PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR('>')		\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)							\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)							\
	PORT_START /* port 0x0a */											\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K10") PORT_CODE(KEYCODE_F10) PORT_CHAR(UCHAR_MAMEKEY(F11))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("_ =") PORT_CODE(KEYCODE_MINUS) PORT_CHAR('_') PORT_CHAR('=')		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("@") PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('@')			\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(": *") PORT_CODE(KEYCODE_QUOTE) PORT_CHAR(':') PORT_CHAR('*')		\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("/ ?") PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/') PORT_CHAR('?')		\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)								\
	PORT_START /* port 0x0b */											\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K11") PORT_CODE(KEYCODE_F11) PORT_CHAR(UCHAR_MAMEKEY(F12))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Blank") PORT_CODE(KEYCODE_EQUALS) PORT_CHAR(UCHAR_MAMEKEY(ESC))	\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("\\ ^") PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR('\\') PORT_CHAR('^')		\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("[ ]") PORT_CODE(KEYCODE_BACKSLASH) PORT_CHAR('[') PORT_CHAR(']')		\
		PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)								\
	PORT_START /* port 0x0c */														\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("WRK") PORT_CODE(KEYCODE_INSERT) PORT_CHAR(UCHAR_MAMEKEY(RCONTROL))			\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("INS PTL") PORT_CODE(KEYCODE_DEL) PORT_CHAR(UCHAR_MAMEKEY(INSERT)) PORT_CHAR(UCHAR_MAMEKEY(TAB))	\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("<-") PORT_CODE(KEYCODE_LEFT) PORT_CHAR(UCHAR_MAMEKEY(LEFT))				\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("|<-") PORT_CODE(KEYCODE_LALT) PORT_CHAR(UCHAR_MAMEKEY(LALT))				\
		PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_UNUSED)											\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)											\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)											\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)											\
	PORT_START /* port 0x0d */											\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C-D") PORT_CODE(KEYCODE_HOME) PORT_CHAR(UCHAR_MAMEKEY(CAPSLOCK))	\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("DEL") PORT_CODE(KEYCODE_END) PORT_CHAR(UCHAR_MAMEKEY(DEL))		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("^\\") PORT_CODE(KEYCODE_UP) PORT_CHAR(UCHAR_MAMEKEY(HOME))		\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("END") PORT_CODE(KEYCODE_DOWN) PORT_CHAR(UCHAR_MAMEKEY(END))		\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL1") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))		\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)								\
	PORT_START /* port 0x0e */											\
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("CLR") PORT_CODE(KEYCODE_PGUP) PORT_CHAR(UCHAR_MAMEKEY(PGUP))		\
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("RCL") PORT_CODE(KEYCODE_PGDN) PORT_CHAR(UCHAR_MAMEKEY(PGDN))		\
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("->") PORT_CODE(KEYCODE_RIGHT) PORT_CHAR(UCHAR_MAMEKEY(RIGHT))		\
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("->|") PORT_CODE(KEYCODE_RALT) PORT_CHAR(UCHAR_MAMEKEY(RALT))		\
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL2") PORT_CODE(KEYCODE_TAB) PORT_CHAR(UCHAR_MAMEKEY(ENTER_PAD))	\
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)								\
	PORT_START /* port 0x0f */											\
		PORT_BIT (0x01, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_UNUSED)								\
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT) PORT_CHAR(UCHAR_MAMEKEY(LSHIFT))	\
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))	\
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Stop") PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))\
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)

#define PMD85_KEYBOARD_RESET												\
	PORT_START /* port 0x10 */											\
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("RST") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(UCHAR_MAMEKEY(BACKSPACE))

#define PMD85_DIPSWITCHES						\
	PORT_START /* port 0x11 */					\
		PORT_CONFNAME( 0x01, 0x00, "Basic ROM Module" )		\
			PORT_CONFSETTING( 0x00, DEF_STR( Off ) )	\
			PORT_CONFSETTING( 0x01, DEF_STR( On ) )		\
		PORT_CONFNAME( 0x02, 0x00, "Tape/V.24" )		\
			PORT_CONFSETTING( 0x00, "Tape" )		\
			PORT_CONFSETTING( 0x02, "V.24" )

#define ALFA_DIPSWITCHES						\
	PORT_START /* port 0x11 */					\
		PORT_CONFNAME( 0x02, 0x00, "Tape/V.24" )		\
			PORT_CONFSETTING( 0x00, "Tape" )		\
			PORT_CONFSETTING( 0x02, "V.24" )

INPUT_PORTS_START (pmd85)
	PMD85_KEYBOARD_MAIN
	PMD85_KEYBOARD_RESET
	PMD85_DIPSWITCHES
INPUT_PORTS_END

INPUT_PORTS_START (alfa)
	PMD85_KEYBOARD_MAIN
	PMD85_KEYBOARD_RESET
	ALFA_DIPSWITCHES
INPUT_PORTS_END

INPUT_PORTS_START (mato)
	PORT_START /* port 0x00 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("1 !") PORT_CODE(KEYCODE_1) PORT_CHAR('1') PORT_CHAR('!')
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("2 \"") PORT_CODE(KEYCODE_2) PORT_CHAR('2') PORT_CHAR('\"')
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("3 #") PORT_CODE(KEYCODE_3) PORT_CHAR('3') PORT_CHAR('#')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("4 $") PORT_CODE(KEYCODE_4) PORT_CHAR('4') PORT_CHAR('$')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("5 %") PORT_CODE(KEYCODE_5) PORT_CHAR('5') PORT_CHAR('%')
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("6 &") PORT_CODE(KEYCODE_6) PORT_CHAR('6') PORT_CHAR('&')
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("7 \'") PORT_CODE(KEYCODE_7) PORT_CHAR('7') PORT_CHAR('\'')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x01 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Q") PORT_CODE(KEYCODE_Q) PORT_CHAR('Q')
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("W") PORT_CODE(KEYCODE_W) PORT_CHAR('W')
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("E") PORT_CODE(KEYCODE_E) PORT_CHAR('E')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("R") PORT_CODE(KEYCODE_R) PORT_CHAR('R')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("T") PORT_CODE(KEYCODE_T) PORT_CHAR('T')
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Y") PORT_CODE(KEYCODE_Y) PORT_CHAR('Y')
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("U") PORT_CODE(KEYCODE_U) PORT_CHAR('U')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x02 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("A") PORT_CODE(KEYCODE_A) PORT_CHAR('A')
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("S") PORT_CODE(KEYCODE_S) PORT_CHAR('S')
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("D") PORT_CODE(KEYCODE_D) PORT_CHAR('D')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("F") PORT_CODE(KEYCODE_F) PORT_CHAR('F')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("G") PORT_CODE(KEYCODE_G) PORT_CHAR('G')
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("H") PORT_CODE(KEYCODE_H) PORT_CHAR('H')
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("J") PORT_CODE(KEYCODE_J) PORT_CHAR('J')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x03 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Z") PORT_CODE(KEYCODE_Z) PORT_CHAR('Z')
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("X") PORT_CODE(KEYCODE_X) PORT_CHAR('X')
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("C") PORT_CODE(KEYCODE_C) PORT_CHAR('C')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("V") PORT_CODE(KEYCODE_V) PORT_CHAR('V')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("B") PORT_CODE(KEYCODE_B) PORT_CHAR('B')
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("N") PORT_CODE(KEYCODE_N) PORT_CHAR('N')
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("M") PORT_CODE(KEYCODE_M) PORT_CHAR('M')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x04 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("= _") PORT_CODE(KEYCODE_MINUS) PORT_CHAR('=') PORT_CHAR('_')
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("0 -") PORT_CODE(KEYCODE_0) PORT_CHAR('0') PORT_CHAR('-')
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("\\ ^") PORT_CODE(KEYCODE_CLOSEBRACE) PORT_CHAR('\\') PORT_CHAR('^')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("8 (") PORT_CODE(KEYCODE_8) PORT_CHAR('8') PORT_CHAR('(')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", <") PORT_CODE(KEYCODE_COMMA) PORT_CHAR(',') PORT_CHAR('<')
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("K") PORT_CODE(KEYCODE_K) PORT_CHAR('K')
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("I") PORT_CODE(KEYCODE_I) PORT_CHAR('I')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x05 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("P") PORT_CODE(KEYCODE_P) PORT_CHAR('P')
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("/") PORT_CODE(KEYCODE_SLASH) PORT_CHAR('/')
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("; +") PORT_CODE(KEYCODE_COLON) PORT_CHAR(';') PORT_CHAR('+')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("9 )") PORT_CODE(KEYCODE_9) PORT_CHAR('9') PORT_CHAR(')')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(", <") PORT_CODE(KEYCODE_STOP) PORT_CHAR('.') PORT_CHAR('<')
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("L") PORT_CODE(KEYCODE_L) PORT_CHAR('L')
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("O") PORT_CODE(KEYCODE_O) PORT_CHAR('O')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x06 */
		PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Down") PORT_CODE(KEYCODE_DOWN) PORT_CHAR(UCHAR_MAMEKEY(DOWN))
		PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Up") PORT_CODE(KEYCODE_UP) PORT_CHAR(UCHAR_MAMEKEY(UP))
		PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Space") PORT_CODE(KEYCODE_SPACE) PORT_CHAR(' ')
		PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("@") PORT_CODE(KEYCODE_OPENBRACE) PORT_CHAR('@')
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("<-") PORT_CODE(KEYCODE_LEFT) PORT_CHAR(UCHAR_MAMEKEY(LEFT))
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("->") PORT_CODE(KEYCODE_RIGHT) PORT_CHAR(UCHAR_MAMEKEY(RIGHT))
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME(": *") PORT_CODE(KEYCODE_QUOTE) PORT_CHAR(':') PORT_CHAR('*')
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x07 */
		PORT_BIT (0x01, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x10, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x20, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x40, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("EOL") PORT_CODE(KEYCODE_ENTER) PORT_CHAR(UCHAR_MAMEKEY(ENTER))
	PORT_START /* port 0x08 */
		PORT_BIT (0x01, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x02, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x04, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT (0x08, IP_ACTIVE_LOW, IPT_UNUSED)
		PORT_BIT(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Stop") PORT_CODE(KEYCODE_LCONTROL) PORT_CHAR(UCHAR_MAMEKEY(LCONTROL))
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_LSHIFT) PORT_CHAR(UCHAR_MAMEKEY(LSHIFT))
		PORT_BIT(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Shift") PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_MAMEKEY(RSHIFT))
		PORT_BIT(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD) PORT_NAME("Continue") PORT_CODE(KEYCODE_RCONTROL) PORT_CHAR(UCHAR_MAMEKEY(RCONTROL))
		PORT_BIT (0x80, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_START /* port 0x09 */
		PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD) PORT_NAME("RST") PORT_CODE(KEYCODE_BACKSPACE) PORT_CHAR(UCHAR_MAMEKEY(BACKSPACE))
INPUT_PORTS_END



/* machine definition */
static MACHINE_DRIVER_START( pmd85 )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", 8080, 2000000)		/* 2.048MHz ??? */
	MDRV_CPU_PROGRAM_MAP(pmd85_mem, 0)
	MDRV_CPU_IO_MAP(pmd85_readport, pmd85_writeport)
	MDRV_FRAMES_PER_SECOND(50)
	MDRV_VBLANK_DURATION(0)
	MDRV_INTERLEAVE(1)

	MDRV_MACHINE_RESET( pmd85 )

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_SIZE(288, 256)
	MDRV_VISIBLE_AREA(0, 288-1, 0, 256-1)
	MDRV_PALETTE_LENGTH(sizeof (pmd85_palette) / 3)
	MDRV_COLORTABLE_LENGTH(sizeof (pmd85_colortable))
	MDRV_PALETTE_INIT( pmd85 )

	MDRV_VIDEO_START( pmd85 )
	MDRV_VIDEO_UPDATE( pmd85 )

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")
	MDRV_SOUND_ADD(WAVE, 0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( pmd852a )
	MDRV_IMPORT_FROM( pmd85 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PROGRAM_MAP(pmd852a_mem, 0)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( pmd853 )
	MDRV_IMPORT_FROM( pmd85 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PROGRAM_MAP(pmd853_mem, 0)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( alfa )
	MDRV_IMPORT_FROM( pmd85 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PROGRAM_MAP(alfa_mem, 0)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( mato )
	MDRV_IMPORT_FROM( pmd85 )
	MDRV_CPU_MODIFY("main")
	MDRV_CPU_PROGRAM_MAP(mato_mem, 0)
	MDRV_CPU_IO_MAP(mato_readport, mato_writeport)
MACHINE_DRIVER_END

ROM_START(pmd851)
	ROM_REGION(0x11000,REGION_CPU1,0)
	ROM_LOAD("pmd85-1.bin", 0x10000, 0x1000, CRC(ef50b416) SHA1(afa3ec0d03228adc5287a4cba905ce7ad0497dff))
	ROM_REGION(0x2400,REGION_USER1,0)
	ROM_LOAD_OPTIONAL("pmd85-1.bas", 0x0000, 0x2400, CRC(4fc37d45) SHA1(3bd0f92f37a3f2ee539916dc75508bda37433a72))
ROM_END

ROM_START(pmd852)
	ROM_REGION(0x11000,REGION_CPU1,0)
	ROM_LOAD("pmd85-2.bin", 0x10000, 0x1000, CRC(d4786f63) SHA1(6facdf37bb012714244b012a0c4bd715a956e42b))
	ROM_REGION(0x2400,REGION_USER1,0)
	ROM_LOAD_OPTIONAL("pmd85-2.bas", 0x0000, 0x2400, CRC(fc4a3ebf) SHA1(3bfc0e9a5cd5187da573b5d539d7246358125a88))
ROM_END

ROM_START(pmd852a)
	ROM_REGION(0x11000,REGION_CPU1,0)
	ROM_LOAD("pmd85-2a.bin", 0x10000, 0x1000, CRC(5a9a961b) SHA1(7363341596367d08b9a98767c6585ce18dfd03af))
	ROM_REGION(0x2400,REGION_USER1,0)
	ROM_LOAD_OPTIONAL("pmd85-2a.bas", 0x0000, 0x2400, CRC(6ff379ad) SHA1(edcaf2420cac9771596ead5c86c41116b228eca3))
ROM_END

ROM_START(pmd852b)
	ROM_REGION(0x11000,REGION_CPU1,0)
	ROM_LOAD("pmd85-2a.bin", 0x10000, 0x1000, CRC(5a9a961b) SHA1(7363341596367d08b9a98767c6585ce18dfd03af))
	ROM_REGION(0x2400,REGION_USER1,0)
	ROM_LOAD_OPTIONAL("pmd85-2a.bas", 0x0000, 0x2400, CRC(6ff379ad) SHA1(edcaf2420cac9771596ead5c86c41116b228eca3))
ROM_END

ROM_START(pmd853)
	ROM_REGION(0x12000,REGION_CPU1,0)
	ROM_LOAD("pmd85-3.bin", 0x10000, 0x2000, CRC(83e22c47) SHA1(5f131e27ae3ec8907adbe5cd228c67d131066084))
	ROM_REGION(0x2800,REGION_USER1,0)
	ROM_LOAD_OPTIONAL("pmd85-3.bas", 0x0000, 0x2800, CRC(1e30e91d) SHA1(d086040abf4c0a7e5da8cf4db7d1668a1d9309a4))
ROM_END

ROM_START(alfa)
	ROM_REGION(0x13400,REGION_CPU1,0)
	ROM_LOAD("alfa.bin", 0x10000, 0x1000, CRC(e425eedb) SHA1(db93b5de1e16b5ae71be08feb083a2ac15759495))
	ROM_LOAD("alfa.bas", 0x11000, 0x2400, CRC(9a73bfd2) SHA1(74314d989846f64e715f64deb84cb177fa62f4a9))
ROM_END

ROM_START(mato)
	ROM_REGION(0x14000,REGION_CPU1,0)
	ROM_LOAD("mato.bin", 0x10000, 0x4000, CRC(574110a6) SHA1(4ff2cd4b07a1a700c55f92e5b381c04f758fb461))
ROM_END

ROM_START(matoh)
	ROM_REGION(0x14000,REGION_CPU1,0)
	ROM_LOAD("matoh.bin", 0x10000, 0x4000, CRC(ca25880d) SHA1(38ce0b6a26d48a09fdf96863c3eaf3705aca2590))
ROM_END

static struct CassetteOptions pmd85_cassette_options = {
	1,		/* channels */
	16,		/* bits per sample */
	7200		/* sample frequency */
};

static void pmd85_cassette_getinfo(const device_class *devclass, UINT32 state, union devinfo *info)
{
	/* cassette */
	switch(state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_COUNT:							info->i = 1; break;

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case DEVINFO_PTR_CASSETTE_FORMATS:				info->p = (void *) pmd85_pmd_format; break;
		case DEVINFO_PTR_CASSETTE_OPTIONS:				info->p = (void *) &pmd85_cassette_options; break;

		/* --- the following bits of info are returned as 64-bit signed integers --- */
		case DEVINFO_INT_CASSETTE_DEFAULT_STATE:		info->i = CASSETTE_STOPPED | CASSETTE_SPEAKER_ENABLED; break;

		default:										cassette_device_getinfo(devclass, state, info); break;
	}
}

SYSTEM_CONFIG_START(pmd85)
	CONFIG_RAM_DEFAULT(64 * 1024)
	CONFIG_DEVICE(pmd85_cassette_getinfo)
SYSTEM_CONFIG_END


/*    YEAR  NAME     PARENT  COMPAT	MACHINE  INPUT  INIT      CONFIG COMPANY  FULLNAME */
COMP( 1985, pmd851,  0,      0,		pmd85,   pmd85, pmd851,   pmd85, "Tesla", "PMD-85.1" , 0)
COMP( 1985, pmd852,  pmd851, 0,		pmd85,   pmd85, pmd851,   pmd85, "Tesla", "PMD-85.2" , 0)
COMP( 1985, pmd852a, pmd851, 0,		pmd852a, pmd85, pmd852a,  pmd85, "Tesla", "PMD-85.2A" , 0)
COMP( 1985, pmd852b, pmd851, 0,		pmd852a, pmd85, pmd852a,  pmd85, "Tesla", "PMD-85.2B" , 0)
COMP( 1988, pmd853,  pmd851, 0,		pmd853,  pmd85, pmd853,   pmd85, "Tesla", "PMD-85.3" , 0)
COMP( 1986, alfa,    pmd851, 0,		alfa,    alfa,  alfa,     pmd85, "Didaktik", "Alfa" , 0)
COMP( 1985, mato,    pmd851, 0,		mato,    mato,  mato,     pmd85, "Statny", "Mato (Basic ROM)" , 0)
COMP( 1989, matoh,   pmd851, 0,		mato,    mato,  mato,     pmd85, "Statny", "Mato (Games ROM)" , 0)
