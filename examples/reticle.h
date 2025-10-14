#ifndef RETICLE_H
#define RETICLE_H

#include <raylib.h>


typedef enum
{
	RETICLE_CENTER_DOT       = 1,
	RETICLE_CIRCLE           = 2,
	RETICLE_CROSSHAIRS       = 4,
	RETICLE_HIDE_TOP_HAIR    = 8,
	RETICLE_HIDE_BOTTOM_HAIR = 16,
	RETICLE_HIDE_LEFT_HAIR   = 32,
	RETICLE_HIDE_RIGHT_HAIR  = 64,
}
ReticleFlags;


static void
drawReticle(
	int          center_x, 
	int          center_y, 
	int          line_thickness,
	int          length, 
	int          spread,
	Color        color,
	ReticleFlags flags
)
{
	if (line_thickness    < 0) line_thickness    = 0;
	int
		h_hair_y   = center_y - (line_thickness/2),
		h_hair_l_x = center_x - length - spread,
		h_hair_r_x = center_x + spread,
		v_hair_x   = center_x - (line_thickness/2),
		v_hair_u_y = center_y - length - spread,
		v_hair_d_y = center_y + spread;

	Vector2 center = (Vector2){center_x, center_y};
	if ((line_thickness & 1)) {
		center = Vector2Add(center, (Vector2){0.5f, 0.5f});
	}
	else {
		h_hair_r_x--;
		v_hair_d_y--;
	}

	if (flags & RETICLE_CROSSHAIRS) {
		if ( !(flags & RETICLE_HIDE_LEFT_HAIR) )
			DrawRectangle(h_hair_l_x,     h_hair_y,       length,         line_thickness, color);
		if ( !(flags & RETICLE_HIDE_RIGHT_HAIR) )
			DrawRectangle(h_hair_r_x + 1, h_hair_y,       length,         line_thickness, color);
		if ( !(flags & RETICLE_HIDE_TOP_HAIR) )
			DrawRectangle(v_hair_x,       v_hair_u_y,     line_thickness, length,         color);
		if ( !(flags & RETICLE_HIDE_BOTTOM_HAIR) )
			DrawRectangle(v_hair_x,       v_hair_d_y + 1, line_thickness, length,         color);
	}
	if (flags & RETICLE_CENTER_DOT)
		DrawRectangle(v_hair_x,     h_hair_y,       line_thickness, line_thickness, color);
	if (flags & RETICLE_CIRCLE)
		DrawRing(center, spread + line_thickness, spread, 0.0f, 360.0f, 32, color);
	
}

#endif /* RETICLE_H */
