#ifndef MENU_H
#define MENU_H


#include "common.h"

typedef void (*MenuAction)(void *data);

typedef struct
MenuItem
{
	char       *str;
	MenuAction  action;
	void       *data;
}
MenuItem;


void
Menu(
	MenuItem *items, 
	size_t   size, 
	Vector2i screen_size, 
	Vector2i item_size, 
	int      padding
)
{
	Vector2i 
		dimensions = {
				item_size.w,
				(item_size.h + padding) * size - padding
			},
		position   = {
				(screen_size.w / 2) - (dimensions.w / 2),
				(screen_size.h / 2) - (dimensions.h / 2)
			};

	int i;
	for (i = 0; i < size; i++) {
		if (
				GuiButton(
						(Rectangle){
							position.x,
							position.y + (i * (item_size.y + padding)), 
							item_size.w, 
							item_size.h
						}, 
						items[i].str
					)
			) 
				if (items[i].action) items[i].action(items[i].data);
	}
}



#endif /* MENU_H */
