/*
================================================================================		
Copyright (C) 2026 by chrisxdeboy@gmail.com

Permission to use, copy, modify, and/or distribute this software for
any purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
================================================================================
	NAME:
		loading_screen.h

	DESCRIPTION:
		A simple immediate mode loading screen based on raygui

	INSTALLATION:
		Drop it in with your headers alongside `raygui.h`

	USAGE:
		Define "LOADING_SCREEN_IMPLEMENTATION" above the import for this header,
		then call
		`LoadingScreen_draw(int progress_percent, const char *resource_string)`
		between resource load calls. with appropriate changes to the arguments.

		You can also redefine the macros in order to modify the size and 
		placement of the loading screen elements.
*/
#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

#include "raygui.h"
#include <raylib.h>


/* LOADING BAR */
#ifndef LOADING_BAR_X_POS
	#define LOADING_BAR_X_POS 20
#endif
#ifndef LOADING_BAR_Y_POS
	#define LOADING_BAR_Y_POS ((screen_height / 2) - (LOADING_BAR_HEIGHT / 2))
#endif
#ifndef LOADING_BAR_WIDTH
	#define LOADING_BAR_WIDTH (screen_width - (LOADING_BAR_X_POS * 3))
#endif
#ifndef LOADING_BAR_HEIGHT
	#define LOADING_BAR_HEIGHT 20
#endif

/* LOADING... TEXT */
#ifndef LOADING_TEXT_X_POS
	#define LOADING_TEXT_X_POS LOADING_BAR_X_POS
#endif
#ifndef LOADING_TEXT_Y_POS
	#define LOADING_TEXT_Y_POS (LOADING_BAR_Y_POS - 25)
#endif
#ifndef LOADING_TEXT_SIZE
	#define LOADING_TEXT_SIZE 20
#endif
#ifndef LOADING_TEXT_COLOR
	#define LOADING_TEXT_COLOR BLACK
#endif

/* LOADING RESOURCE TEXT */
#ifndef LOADING_RES_TEXT_X_POS
	#define LOADING_RES_TEXT_X_POS LOADING_BAR_X_POS
#endif
#ifndef LOADING_RES_TEXT_Y_POS
	#define LOADING_RES_TEXT_Y_POS (LOADING_BAR_Y_POS + LOADING_BAR_HEIGHT + 5)
#endif
#ifndef LOADING_RES_TEXT_SIZE
	#define LOADING_RES_TEXT_SIZE 10
#endif
#ifndef LOADING_RES_TEXT_COLOR
	#define LOADING_RES_TEXT_COLOR GRAY
#endif


void LoadingScreen_draw(float progress_percent, const char *resource_string);


#endif /* LOADING_SCREEN_H */
#ifdef LOADING_SCREEN_IMPLEMENTATION

void 
LoadingScreen_draw(float progress_percent, const char *resource_string)
{
	static int num_elipses = 0;
	const int
		screen_width  = GetScreenWidth(),
		screen_height = GetScreenHeight();
		
	char 
		loading_str[sizeof("Loading...")] = "Loading",
		percent_str[16];
		
	for (int i = 0; i < num_elipses; i++) loading_str[7+i] = '.';
	loading_str[7 + num_elipses] = '\0';

	num_elipses = ++num_elipses % 4;
	snprintf(percent_str, sizeof(percent_str), "%i%%", (int)progress_percent);
	
	BeginDrawing();
		ClearBackground(GetColor(GuiGetStyle(DEFAULT, BACKGROUND_COLOR)));

		GuiProgressBar(
				(Rectangle){
						LOADING_BAR_X_POS,
						LOADING_BAR_Y_POS,
						LOADING_BAR_WIDTH,
						LOADING_BAR_HEIGHT
					},
				NULL,
				percent_str,
				&progress_percent, 
				0.0f, 
				100.0f
			);
		DrawText(
				loading_str,
				LOADING_TEXT_X_POS,
				LOADING_TEXT_Y_POS,
				LOADING_TEXT_SIZE,
				LOADING_TEXT_COLOR
			);
		if (resource_string)
			DrawText(
					resource_string,
					LOADING_RES_TEXT_X_POS,
					LOADING_RES_TEXT_Y_POS,
					LOADING_RES_TEXT_SIZE,
					LOADING_RES_TEXT_COLOR
				);
	EndDrawing();
}

#endif /* LOADING_SCREEN_IMPLEMENTATION */
