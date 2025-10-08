/*
================================================================================		
Copyright (C) 2025 by chrisxdeboy@gmail.com

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
		menu.h

	DESCRIPTION:
		A simple menu system based on raygui

	INSTALLATION:
		Drop it in with your headers alongside `raygui.h`

	USAGE:
		Use the `Menu(label, ...)` macro function like so to declare a `Menu` 
		object:
		```c
			Menu mainMenu = Menu( "Main Menu",
				(MenuItem){ "Run",     runEngine,  engine},
				(MenuItem){ "Options", switchMenu, &optionsMenu},
				(MenuItem){ "Exit",    closeAll,   engine}
			);
		```

		And put this somewhere in your raylib code between `BeginDrawing()` and 
		`EndDrawing()` to draw the menu:
		```c
			Menu_draw(
				currentMenu,
				SCREEN_WIDTH, 
				SCREEN_HEIGHT,
				220, 
				30,
				10,
				&selection,
				GET_KEY_OR_BUTTON_PRESSED(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT, KEY_ENTER)
			);
		```
*/
#ifndef MENU_H
#define MENU_H


#include "raygui.h"
#include <raylib.h>


#ifndef MENU_LABEL_FONT_SIZE
	#define MENU_LABEL_FONT_SIZE 10
#endif

#define Menu( label_str, ... ) \
	(Menu){ \
		(label_str), \
		sizeof((MenuItem[]){__VA_ARGS__}) / sizeof(MenuItem), \
		-1, \
		(MenuItem[]){__VA_ARGS__} \
	}


typedef void (*MenuAction)(void *data);

typedef struct
MenuItem
{
	char       *label;
	MenuAction  action;
	void       *data;
}
MenuItem;

typedef struct
Menu
{
	char     *label;
	size_t    size;
	int       selection;
	MenuItem *items;
}
Menu;


static inline void
Menu_draw(
	Menu     *menu, 
	int       screen_width, 
	int       screen_height,
	int       item_width,
	int       item_height, 
	int       padding,
	int       selection,
	int       selected
)
{
	MenuItem *items = menu->items;
	int 
		i,
		dim_w = item_width,
		dim_h = (item_height + padding) * menu->size - padding,
		pos_x = (screen_width / 2) - (dim_w / 2),
		pos_y = (screen_height / 2) - (dim_h / 2);

	DrawText(
			menu->label, 
			pos_x, 
			pos_y - MENU_LABEL_FONT_SIZE - padding, 
			MENU_LABEL_FONT_SIZE, 
			BLACK
		);

	if (selection != 0)
	{
		menu->selection += selection;
		if (menu->selection <  0)               menu->selection = menu->size - 1;
		if (menu->size      <= menu->selection) menu->selection = 0;
	}
	
	for (i = 0; i < menu->size; i++) {
		Rectangle rectangle = (Rectangle){
				pos_x,
				pos_y + (i * (item_height + padding)), 
				item_width, 
				item_height
			};
		
		if (CheckCollisionPointRec(GetMousePosition(), rectangle))
			menu->selection = i;
		
		if (i == menu->selection) {
			GuiSetState(STATE_FOCUSED);
			if (selected && items[i].action) items[i].action(items[i].data);
		}
		else GuiSetState(STATE_NORMAL);

		if (GuiButton(rectangle, items[i].label)) 
			if (items[i].action) items[i].action(items[i].data);
	}
}



#endif /* MENU_H */
