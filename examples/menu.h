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
				GetScreenWidth(),
				GetScreenHeight(),
				MENU_WIDTH,
				MENU_ITEM_HEIGHT,
				MENU_PADDING,
				GET_KEY_OR_BUTTON_AXIS_PRESSED(
						0, 
						GAMEPAD_BUTTON_LEFT_FACE_DOWN, 
						KEY_DOWN, 
						GAMEPAD_BUTTON_LEFT_FACE_UP, 
						KEY_UP
					),
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


#define MANAGE_INPUT( item_prefix, value ) \
	if (i == menu->selection) { \
		GuiSetState(STATE_FOCUSED); \
		if (selected && items[i].(item_prefix).action) items[i].(item_prefix).action(items[i].(item_prefix).data, (value)); \
	} \
	else GuiSetState(STATE_NORMAL);


#ifdef ON_CONSOLE
	#define Menu( label_str, item_width, item_height, padding, ... ) \
		(Menu){ \
			(label_str), \
			(item_width), \
			(item_height), \
			(padding), \
			0, \
			sizeof((MenuItem[]){__VA_ARGS__}) / sizeof(MenuItem), \
			(MenuItem[]){__VA_ARGS__} \
		}
#else
	#define Menu( label_str, item_width, item_height, padding, ... ) \
		(Menu){ \
			(label_str), \
			(item_width), \
			(item_height), \
			(padding), \
			-1, \
			sizeof((MenuItem[]){__VA_ARGS__}) / sizeof(MenuItem), \
			(MenuItem[]){__VA_ARGS__} \
		}
#endif

#define MenuLabel( label_str ) \
	{ MENU_LABEL, {(label_str)} }

#define MenuButton( label_str, action, data ) \
	{ \
		MENU_BUTTON, \
		{ \
			(label_str), \
			(action), \
			(data) \
		} \
	}

#define MenuCheckBox( label_str, checked, action, data) \
	{ \
		MENU_CHECKBOX, \
		{ \
			(label_str), \
			(action), \
			(data), \
			(checked) \
		} \
	}

#define MenuComboBox( entries, value, action, data ) \
	{ \
		MENU_COMBO, \
		{ \
			(entries),\
			(value), \
			(action), \
			(data) \
		} \
	}

#define MenuDropDown( entries, value, action, data) \
	{ \
		MENU_DROPDOWN, \
		{ \
			(entries), \
			(value), \
			false, \
			(action), \
			(data) \
		} \
	}
	
#define MenuSlider( label, value, min_val, max_val, action, data) \
	{ \
		MENU_SLIDER, \
		{ \
			(label_left), \
			(value), \
			(min_val), \
			(max_val), \
			(action), \
			(data) \
		} \
	}
	
#define MenuSliderBar( label, value, min_val, max_val, action, data) \
	{ \
		MENU_SLIDERBAR, \
		{ \
			(label_left), \
			(value), \
			(min_val), \
			(max_val), \
			(action), \
			(data) \
		} \
	}

#define MenuTextBox( char_array, action, data) \
	{ \
		MENU_TEXTBOX, \
		{ \
			(char_array), \
			false, \
			(action), \
			(data) \
		} \
	}


typedef void (*MenuAction)(void *data, void *value);


typedef enum
{
	MENU_LABEL,
	MENU_BUTTON,
	MENU_CHECKBOX,
	MENU_COMBO,
	MENU_DROPDOWN,
	MENU_SLIDER,
	MENU_SLIDERBAR,
	MENU_TEXTBOX,
}
MenuItems;


typedef struct
{
	char *label;
}
MenuLabel;

typedef struct
{
	char       *label;
	MenuAction  action;
	void       *data;
}
MenuButton;

typedef struct
{
	char       *label;
	bool        value;
	MenuAction  action;
	void       *data;
}
MenuCheckBox;

typedef struct
{
	char       *label;
	int         value;
	MenuAction  action;
	void       *data;
}
MenuComboBox;

typedef struct
{
	char       *label;
	int         value;
	bool        edit_mode;
	MenuAction  action;
	void       *data;
}
MenuDropdownBox;

typedef struct
{
	char       *label;
	int       
				value,
				min,
				max;
	MenuAction  action;
	void       *data;
}
MenuSlider;

typedef struct
{
	char       *label;
	float       
				value,
				min,
				max;
	MenuAction  action;
	void       *data;
}
MenuSliderBar;

typedef struct
{
	char       *text;
	bool        edit_mode;
	MenuAction  action;
	void       *data;
}
MenuTextBox;


typedef struct
{
	MenuItems type;
	union {
		MenuButton      btn;
		MenuLabel       lbl;
		MenuCheckBox    chk;
		MenuComboBox    cbo;
		MenuDropdownBox ddb;
		MenuSlider      sld;
		MenuSliderBar   sdb;
		MenuTextBox     txb;
	};
}
MenuItem;

typedef struct
Menu
{
	char     *label;
	int       
			item_width,
			item_height,
			padding, 
			selection;
	size_t    size;
	MenuItem *items;
}
Menu;

void 
Menu_draw(
	Menu     *menu, 
	int       screen_width, 
	int       screen_height,
	int       selection,
	int       dial_pressed,
	int       dial_down,
	bool      selected
);


#endif /* MENU_H */
#ifdef MENU_IMPLEMENTATION
	
int
countEntries(char *entries)
{
	int 
		i      = 0,
		result = 0;
	
	while(entries[i]) {
		if (entries[i] == ';') result++;
	}
	return result;
}

void
Menu_draw(
	Menu     *menu, 
	int       screen_width, 
	int       screen_height,
	int       selection,
	int       dial_pressed,
	int       dial_down,
	bool      selected
)
{
	MenuItem *items = menu->items;
	int 
		i,
		item_width  = menu->item_width,
		item_height = menu->item_height,
		padding     = menu->padding,
		dim_w       = item_width,
		dim_h       = (item_height + padding) * menu->size - padding,
		pos_x       = (screen_width / 2) - (dim_w / 2),
		pos_y       = (screen_height / 2) - (dim_h / 2);

	GuiGroupBox(
			(Rectangle){
				pos_x - padding, 
				pos_y - padding, 
				item_width + (2 * padding),
				dim_h + (2 * padding)
			},
			menu->label
		);

	if (selection != 0) {
		menu->selection += selection;
		if (menu->selection <  0)               menu->selection = menu->size - 1;
		if (menu->size      <= menu->selection) menu->selection = 0;
		/* Skip labels - find next button */
		int direction = (selection > 0) - (selection < 0);  // Branchless sign
		int attempts = 0;
		while (items[menu->selection].type == MENU_LABEL && attempts < menu->size) {
			menu->selection += direction;
			if (menu->selection <  0)               menu->selection = menu->size - 1;
			if (menu->size      <= menu->selection) menu->selection = 0;
			attempts++;
		}
	}
	
	for (i = 0; i < menu->size; i++) {
		Rectangle rectangle = (Rectangle){
				pos_x,
				pos_y + (i * (item_height + padding)), 
				item_width, 
				item_height
			};

		MenuItems item_type = items[i].type;
		
		if (
			CheckCollisionPointRec(GetMousePosition(), rectangle) 
			&& item_type != MENU_LABEL
		)
			menu->selection = i;
		


		switch(item_type)  {
		case MENU_LABEL:
			GuiSetState(STATE_NORMAL);
			GuiLabel(rectangle, items[i].lbl.label);
			break;
		case MENU_BUTTON:
			if (i == menu->selection) { 
				GuiSetState(STATE_FOCUSED); 
				if (selected && items[i].btn.action) items[i].btn.action(items[i].btn.data, NULL); 
			} 
			else GuiSetState(STATE_NORMAL);
			
			if (GuiButton(rectangle, items[i].btn.label)) 
				if (items[i].btn.action) items[i].btn.action(items[i].btn.data, NULL);
			break;
		case MENU_CHECKBOX:
			if (i == menu->selection) { 
				GuiSetState(STATE_FOCUSED); 
				if (selected && items[i].chk.action) {
					items[i].chk.value = !items[i].chk.value;
					items[i].chk.action(items[i].chk.data, &items[i].chk.value); 
				}
			}
			else GuiSetState(STATE_NORMAL);

			if (GuiCheckBox(rectangle, items[i].chk.label, &items[i].chk.value)) {
				items[i].chk.value = !items[i].chk.value;
				items[i].chk.action(items[i].chk.data, &items[i].chk.value); 
			}
			break;
		case MENU_COMBO:
			if (i == menu->selection) { 
				GuiSetState(STATE_FOCUSED); 
				if (selected && items[i].cbo.action) {
					int entries = countEntries(items[i].cbo.label);
					items[i].cbo.value++;
					if (entries < items[i].cbo.value) items[i].cbo.value = 0;
					items[i].cbo.action(items[i].cbo.data, &items[i].cbo.value);
				}
			}
			else GuiSetState(STATE_NORMAL);

			if (GuiComboBox(rectangle, items[i].cbo.label, &items[i].cbo.value)) {
				items[i].cbo.action(items[i].cbo.data, &items[i].cbo.value);
			}
			break;
		case MENU_DROPDOWN:
			
			if (i == menu->selection) { 
				GuiSetState(STATE_FOCUSED); 
				if (selected && items[i].ddb.action) {
					items[i].ddb.edit_mode = !items[i].ddb.edit_mode;
				}
			}
			else GuiSetState(STATE_NORMAL);

			if (GuiDropdownBox(rectangle, items[i].ddb.label, &items[i].ddb.value, items[i].ddb.edit_mode)) {
				items[i].ddb.edit_mode = !items[i].ddb.edit_mode;
				items[i].ddb.action(items[i].ddb.data, &items[i].ddb.value);
			}
			break;
		case MENU_SLIDER:
		case MENU_SLIDERBAR:
		case MENU_TEXTBOX:
		}
	}
}

#endif /* MENU_IMPLEMENTATION */
