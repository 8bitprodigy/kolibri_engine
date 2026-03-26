#include <stdlib.h>

#include "dynamicarray.h"
#include "entity.h"
#include "ribbon.h"
/* MUST COME LAST */
#include <raymath.h>
#include <rlgl.h>


RibbonInfo *
RibbonInfo_new(
	float      width,
	float      lifetime,
	Color      color,
	Texture2D  atlas,
	size_t     num_frames
)
{
	RibbonInfo *info;
	if (!(info = malloc(sizeof(RibbonInfo)))) {
		ERR_OUT("Failed to allocate memory for RibbonInfo.");
		return NULL;
	}

	info->width      = width;
	info->lifetime   = lifetime;
	info->color      = color;
	info->atlas      = atlas;
	info->y_scale    = ((float)atlas.height / (float)atlas.width) * (float)num_frames;
	info->num_frames = num_frames;

	return info;
}


void
RenderRibbon(
	Renderable *renderable,
	void       *render_data,
	Vector3     position,
	Camera3D   *camera
)
{
	(void)position; /* ribbon uses start/end directly, not entity position */

//	DBG_OUT("RenderRibbon called");
	
	RibbonInfo *info   = (RibbonInfo*)renderable->data;
	Entity     *entity = (Entity*)render_data;
	RibbonData *data   = (RibbonData*)entity->local_data;
//	DBG_OUT("playing: %d\n", data->playing);
	if (!data->playing) return;

	Vector3 start = data->start;
	Vector3 end   = data->end;

	/* ---- Geometry ------------------------------------------------- */

	Vector3 dir = Vector3Subtract(end, start);
	float length = Vector3Length(dir);
	/*DBG_OUT("start: %.2f %.2f %.2f", data->start.x, data->start.y, data->start.z);
	DBG_OUT("end: %.2f %.2f %.2f", data->end.x, data->end.y, data->end.z);
	DBG_OUT("Length: %.8f", length);
	*/
	if (length < 0.0001f) return;
	
	dir = Vector3Scale(dir, 1.0f / length);

	/*
	 *  Build the width axis: perpendicular to the beam and pointing
	 *  toward the camera. This is the core of immediate-mode ribbon
	 *  rendering — no quaternion, no up ambiguity, just the three
	 *  points we already have.
	 */
	Vector3 to_cam = Vector3Normalize(
			Vector3Subtract(camera->position, start)
		);
	Vector3 right = Vector3Normalize(
			Vector3CrossProduct(dir, to_cam)
		);

	float half_width = (info->width + data->width_delta) * 0.5f;
	
/*	DBG_OUT(
		"Start: %.2f %.2f %.2f  End: %.2f %.2f %.2f  Right: %.2f %.2f %.2f  Half Width: %.2f\n",
		start.x,  start.y,  start.z,
		end.x,    end.y,    end.z,
		right.x,  right.y,  right.z,
		half_width
	);*/
	/* ---- UV ------------------------------------------------------- */

	/*
	 *  Frames are laid out along X. Each frame occupies
	 *  1/num_frames of the texture width.
	 */
	float frame_w  = 1.0f / (float)info->num_frames;
	float u_left   = (float)data->current_frame * frame_w;
	float u_right  = u_left + frame_w;

	/*
	 *  V tiles along the beam. scroll_offset shifts the whole ribbon
	 *  along its length — increment it in the update tick for scroll.
	 *  The texture wraps naturally since it's designed to tile on V.
	 */
	float v_start = data->scroll_offset;
	float v_end   = data->scroll_offset + length / info->y_scale;
	
	if (data->flip_u) {
		float tmp = u_left;
		u_left  = u_right;
		u_right = tmp;
	}
	if (data->flip_v) {
		float tmp = v_start;
		v_start = v_end;
		v_end   = tmp;
	}
	
	/* ---- Color ---------------------------------------------------- */

	/*
	 *  If a colors array is provided it overrides info->color.
	 *  For a single quad with no subdivisions we just use colors[0]
	 *  if present, else info->color.
	 *  When subdivision support is added, colors[i] will tint each
	 *  node segment independently.
	 */
	Color col = (data->colors) ? data->colors[0] : info->color;

	/* ---- Draw ----------------------------------------------------- */

	/*
	 *  Without subdivision offsets, the ribbon is a single quad:
	 *
	 *   TL----TR
	 *   |    / |
	 *   |   /  |
	 *   |  /   |
	 *   BL----BR
	 *
	 *  TL/TR = start ± right * half_width
	 *  BL/BR = end   ± right * half_width
	 *
	 *  When data->offsets is non-NULL, each node along the beam is
	 *  displaced by offsets[i] in the (right, up=dir×right) plane,
	 *  turning the quad into a strip of segments. Not yet implemented.
	 */

	Vector3
		tl = Vector3Subtract(start, Vector3Scale(right, half_width)),
		tr = Vector3Add     (start, Vector3Scale(right, half_width)),
		bl = Vector3Subtract(end,   Vector3Scale(right, half_width)),
		br = Vector3Add     (end,   Vector3Scale(right, half_width));

	float r = col.r / 255.0f;
	float g = col.g / 255.0f;
	float b = col.b / 255.0f;
	float a = col.a / 255.0f;

	rlSetTexture(info->atlas.id);
	rlBegin(RL_QUADS);

		rlColor4ub(col.r, col.g, col.b, col.a);

		rlTexCoord2f(u_left,  v_start); rlVertex3f(tl.x, tl.y, tl.z);
		rlTexCoord2f(u_right, v_start); rlVertex3f(tr.x, tr.y, tr.z);
		rlTexCoord2f(u_right, v_end);   rlVertex3f(br.x, br.y, br.z);
		rlTexCoord2f(u_left,  v_end);   rlVertex3f(bl.x, bl.y, bl.z);

	rlEnd();
	rlSetTexture(0);
/*
	DBG_EXPR(
			// Outline the quad 
			DrawLine3D(tl, tr, RED);
			DrawLine3D(tr, br, RED);
			DrawLine3D(br, bl, RED);
			DrawLine3D(bl, tl, RED);
			// Center line from start to end 
			DrawLine3D(start, end, YELLOW);
		);
*/
}
