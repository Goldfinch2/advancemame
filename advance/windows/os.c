/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2002, 2003, 2005, 2008 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#include "portable.h"

#include "os.h"
#include "log.h"
#include "ksdl.h"
#include "isdl.h"
#include "msdl.h"
#include "target.h"
#include "file.h"
#include "ossdl.h"
#include "snstring.h"
#include "measure.h"
#include "oswin.h"

#include "SDL.h"
#include "SDL_syswm.h"

#include <windows.h>

struct os_context {
	int is_quit; /**< Is termination requested. */
	char title_buffer[128]; /**< Title of the window. */
	HHOOK g_hKeyboardHook;
};

static struct os_context OS;

/***************************************************************************/
/* hook */

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	int bEatKeystroke;
	KBDLLHOOKSTRUCT* p;

	if (nCode < 0 || nCode != HC_ACTION )  // do not process message
		return CallNextHookEx(OS.g_hKeyboardHook, nCode, wParam, lParam);
 
	bEatKeystroke = 0;
	p = (KBDLLHOOKSTRUCT*)lParam;

	switch (wParam) {
	case WM_KEYDOWN:
	case WM_KEYUP:
		bEatKeystroke = (p->vkCode == VK_LWIN) || (p->vkCode == VK_RWIN);
		break;
	}
 
	if (bEatKeystroke)
		return 1;
	else
		return CallNextHookEx(OS.g_hKeyboardHook, nCode, wParam, lParam);
}

void os_internal_ignore_hot_key(void)
{
	// keyboard hook to disable win keys
	log_std(("os: SetWindowsHookEx()\n"));
	if (!OS.g_hKeyboardHook) {
		OS.g_hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL,  LowLevelKeyboardProc, GetModuleHandle(0), 0);
	}
}

void os_internal_restore_hot_key(void)
{
	log_std(("os: UnhookWindowsHookEx()\n"));
	if (OS.g_hKeyboardHook) {
		UnhookWindowsHookEx(OS.g_hKeyboardHook);
		OS.g_hKeyboardHook = 0;
	}
}

/***************************************************************************/
/* Init */

int os_init(adv_conf* context)
{
	memset(&OS, 0, sizeof(OS));

	return 0;
}

void os_done(void)
{
}

static void os_wait(void)
{
	Sleep(1);
}

int os_inner_init(const char* title)
{
	SDL_version compiled;
	target_clock_t start, stop;
	double delay_time;
	unsigned i;
	
	target_yield();

	delay_time = adv_measure_step(os_wait, 0.0001, 0.2, 7);

	if (delay_time > 0) {
		log_std(("os: sleep granularity %g\n", delay_time));
		target_usleep_granularity(delay_time * 1000000);
	} else {
		log_std(("ERROR:os: sleep granularity NOT measured\n"));
		target_usleep_granularity(20000 /* 20 ms */);
	}

	log_std(("os: sys SDL\n"));

	/* print the compiler version */
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
#define COMPILER_RESOLVE(a) #a
#define COMPILER(a, b, c) COMPILER_RESOLVE(a) "." COMPILER_RESOLVE(b) "." COMPILER_RESOLVE(c)
	log_std(("os: compiler GNU %s\n", COMPILER(__GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)));
#else
	log_std(("os: compiler unknown\n"));
#endif

#ifdef USE_LSB
	log_std(("os: compiled little endian system\n"));
#else
	log_std(("os: compiled big endian system\n"));
#endif

	log_std(("os: SDL_Init(SDL_INIT_NOPARACHUTE)\n"));
	if (SDL_Init(SDL_INIT_NOPARACHUTE) != 0) {
		log_std(("os: SDL_Init() failed, %s\n", SDL_GetError()));
		target_err("Error initializing the SDL video support.\n");
		return -1;
	}

	SDL_VERSION(&compiled);

	log_std(("os: compiled with sdl %d.%d.%d\n", compiled.major, compiled.minor, compiled.patch));
	log_std(("os: linked with sdl %d.%d.%d\n", SDL_Linked_Version()->major, SDL_Linked_Version()->minor, SDL_Linked_Version()->patch));
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		log_std(("os: sdl little endian system\n"));
	else
		log_std(("os: sdl big endian system\n"));

	start = target_clock();
	stop = target_clock();
	while (stop == start)
		stop = target_clock();
	log_std(("os: clock delta %ld\n", (unsigned long)(stop - start)));

	/* set the titlebar */
	sncpy(OS.title_buffer, sizeof(OS.title_buffer), title);

	/* set some signal handlers */
	signal(SIGABRT, (void (*)(int))os_signal);
	signal(SIGFPE, (void (*)(int))os_signal);
	signal(SIGILL, (void (*)(int))os_signal);
	signal(SIGINT, (void (*)(int))os_signal);
	signal(SIGSEGV, (void (*)(int))os_signal);
	signal(SIGTERM, (void (*)(int))os_signal);

	return 0;
}

void os_inner_done(void)
{
	log_std(("os: SDL_Quit()\n"));
	SDL_Quit();
}

void os_poll(void)
{
	SDL_Event event;

	/* The event queue works only with the video initialized */
	if (!SDL_WasInit(SDL_INIT_VIDEO))
		return;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_KEYDOWN :
#ifdef USE_KEYBOARD_SDL
				keyb_sdl_event_press(event.key.keysym.sym);
#endif
#ifdef USE_INPUT_SDL
				inputb_sdl_event_press(event.key.keysym.sym);
#endif

				/* toggle fullscreen check */
				if (event.key.keysym.sym == SDLK_RETURN
					&& (event.key.keysym.mod & KMOD_ALT) != 0) {
					if (SDL_WasInit(SDL_INIT_VIDEO) && SDL_GetVideoSurface()) {
						SDL_WM_ToggleFullScreen(SDL_GetVideoSurface());

						if ((SDL_GetVideoSurface()->flags & SDL_FULLSCREEN) != 0) {
							SDL_ShowCursor(SDL_DISABLE);
						} else {
							SDL_ShowCursor(SDL_ENABLE);
						}
					}
				}
			break;
			case SDL_SYSWMEVENT :
			break;
			case SDL_KEYUP :
#ifdef USE_KEYBOARD_SDL
				keyb_sdl_event_release(event.key.keysym.sym);
#endif
#ifdef USE_INPUT_SDL
				inputb_sdl_event_release(event.key.keysym.sym);
#endif
			break;
			case SDL_MOUSEMOTION :
#ifdef USE_MOUSE_SDL
				mouseb_sdl_event_move(event.motion.xrel, event.motion.yrel);
#endif
			break;
			case SDL_MOUSEBUTTONDOWN :
#ifdef USE_MOUSE_SDL
				if (event.button.button > 0)
					mouseb_sdl_event_press(event.button.button-1);
#endif
			break;
			case SDL_MOUSEBUTTONUP :
#ifdef USE_MOUSE_SDL
				if (event.button.button > 0)
					mouseb_sdl_event_release(event.button.button-1);
#endif
			break;
			case SDL_QUIT :
				OS.is_quit = 1;
				break;
		}
	}
}

void* os_internal_sdl_get(void)
{
	return &OS;
}

const char* os_internal_sdl_title_get(void)
{
	return OS.title_buffer;
}

void* os_internal_window_get(void)
{
	SDL_SysWMinfo info;

	SDL_VERSION(&info.version);

	if (!SDL_GetWMInfo(&info))
		return 0;

	return info.window;
}

/***************************************************************************/
/* Signal */

int os_is_quit(void)
{
	return OS.is_quit;
}

void os_default_signal(int signum, void* info, void* context)
{
	log_std(("os: signal %d\n", signum));

#if defined(USE_VIDEO_SDL) || defined(USE_VIDEO_SVGAWIN)
	log_std(("os: adv_video_abort\n"));
	{
		extern void adv_video_abort(void);
		adv_video_abort();
	}
#endif

#if defined(USE_SOUND_SDL)
	log_std(("os: sound_abort\n"));
	{
		extern void soundb_abort(void);
		soundb_abort();
	}
#endif

	SDL_Quit();

	target_mode_reset();

	log_std(("os: close log\n"));
	log_abort();

	target_signal(signum, info, context);
}

/***************************************************************************/
/* Extension */

typedef BOOLEAN (WINAPI* HidD_GetAttributes_type)(HANDLE HidDeviceObject, PHIDD_ATTRIBUTES Attributes);

int GetRawInputDeviceHIDInfo(const char* name, unsigned* vid, unsigned* pid, unsigned* rev)
{
	char* buffer;
	const char* last;
	HANDLE h, l;
	HIDD_ATTRIBUTES attr;
	HidD_GetAttributes_type HidD_GetAttributes_ptr;
	
	last = strrchr(name, '\\');
	if (!last)
		last = name;
	else
		++last;

	l = LoadLibrary("HID.DLL");
	if (!l) {
		log_std(("ERROR:GetRawInputDeviceHIDInfo: error loading HID.DLL\n"));
		goto err;
	}

	HidD_GetAttributes_ptr = (HidD_GetAttributes_type)GetProcAddress(l, "HidD_GetAttributes");
	if (!HidD_GetAttributes_ptr) {
		log_std(("ERROR:GetRawInputDeviceHIDInfo: error getting HidD_GetAttributes\n"));
		goto err_unload;
	}

	buffer = malloc(16 + strlen(name));
	strcpy(buffer, "\\\\?\\");
	strcat(buffer, last);

	h = CreateFile(buffer, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);	
	if (h == INVALID_HANDLE_VALUE) {
		log_std(("ERROR:GetRawInputDeviceHIDInfo: error %d in CreateFile(%s)\n", (unsigned)GetLastError(), buffer));
		goto err_free;
	}

	memset(&attr, 0, sizeof(HIDD_ATTRIBUTES));
	attr.Size = sizeof(HIDD_ATTRIBUTES);
	if (!HidD_GetAttributes_ptr(h, &attr)) {
		if (GetLastError() != ERROR_INVALID_FUNCTION)
			log_std(("ERROR:GetRawInputDeviceHIDInfo: error %d in HidD_GetAttributes(%s)\n", (unsigned)GetLastError(), buffer));
		goto err_free;
	}

	*vid = attr.VendorID;
	*pid = attr.ProductID;
	*rev = attr.VersionNumber;

	free(buffer);
	FreeLibrary(l);

	return 0;

err_close:
	CloseHandle(h);
err_free:
	free(buffer);
err_unload:
	FreeLibrary(l);
err:
	return -1;
}

/***************************************************************************/
/* Main */

int main(int argc, char* argv[])
{
	if (target_init() != 0)
		return EXIT_FAILURE;

	if (file_init() != 0) {
		target_done();
		return EXIT_FAILURE;
	}

	if (os_main(argc, argv) != 0) {
		file_done();
		target_done();
		return EXIT_FAILURE;
	}

	file_done();
	target_done();

	return EXIT_SUCCESS;
}

