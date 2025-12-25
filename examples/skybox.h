/*
	SKYBOX.H

	DESCRIPTION:
		A simple, single-header library based on Raylib to draw a skybox.

	LICENSE:
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


	USAGE:
		1. Create an array of 6 Texture2Ds (note the order laid out in 
			SkyboxSides)
		2. For best results, set filtering to TEXTURE_FILTER_BILINEAR and wrap 
			to TEXTURE_WRAP_CLAMP, as that will eliminate the appearance of 
			seams
		2. Call SkyBos_draw() before rendering anything else, then clear the 
			depth buffer
*/

#ifndef SKYBOX_H
#define SKYBOX_H


#include <raymath.h>
#include <rlgl.h>


typedef enum
{
	SKYBOX_UP,
	SKYBOX_DOWN,
	SKYBOX_NORTH,
	SKYBOX_SOUTH,
	SKYBOX_EAST,
	SKYBOX_WEST,
}
SkyboxSides;

#ifndef SKYBOX_NAMES_OVERRIDDEN
static const char *SkyBox_names[6] = {
		"UP",
		"DOWN",
		"NORTH",
		"SOUTH",
		"EAST",
		"WEST"
	};
#endif /* SKYBOX_NAMES_OVERRIDDEN */

static const Vector3 SkyBox_verts[8] = {
		/* Top (+Y) */
		(Vector3){-1.0f,  1.0f, -1.0f}, /* 0  NW */
		(Vector3){ 1.0f,  1.0f, -1.0f}, /* 1  NE */
		(Vector3){ 1.0f,  1.0f,  1.0f}, /* 2  SE */
		(Vector3){-1.0f,  1.0f,  1.0f}, /* 3  SW */
		/* Bottom (-Y) */
		(Vector3){-1.0f, -1.0f, -1.0f}, /* 4  NW */
		(Vector3){ 1.0f, -1.0f, -1.0f}, /* 5  NE */
		(Vector3){ 1.0f, -1.0f,  1.0f}, /* 6  SE */
		(Vector3){-1.0f, -1.0f,  1.0f}, /* 7  SW */
	};

static const Vector3 SkyBox_normals[6] = {
		(Vector3){ 0.0f, -1.0f,  0.0f}, /* Up */
		(Vector3){ 0.0f,  1.0f,  0.0f}, /* Down */
		(Vector3){ 0.0f,  0.0f,  1.0f}, /* North */
		(Vector3){ 0.0f,  0.0f, -1.0f}, /* South */
		(Vector3){-1.0f,  0.0f,  0.0f}, /* East */
		(Vector3){ 1.0f,  0.0f,  0.0f}, /* West */
	};

static const int SkyBox_faces[6][4] = {
		{0, 1, 2, 3}, /* Up (+Y) */
		{7, 6, 5, 4}, /* Down (-Y) */
		{1, 0, 4, 5}, /* North (-Z) */
		{3, 2, 6, 7}, /* South (+Z) */
		{2, 1, 5, 6}, /* East (+X) */
		{0, 3, 7, 4}, /* West (-X) */
	};

static void
SkyBox_draw(Camera *camera, Texture2D textures[6], Quaternion orientation)
{
	Vector3 forward = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
	Matrix  rot     = QuaternionToMatrix(orientation);
	
	rlDisableDepthTest();
	rlDisableDepthMask(); 

	rlPushMatrix();
        /* center on camera so skybox moves with camera */
        rlTranslatef(camera->position.x, camera->position.y, camera->position.z);

        /* keep depth writes off so it doesn't occlude world */

			rlColor4ub(255, 255, 255, 255);
			for (int f = 0; f < 6; ++f) {
				rlBegin(RL_QUADS);
				/* Skip faces the camera is not facing 
				Vector3 n = Vector3Transform(SkyBox_normals[f], rot);
				if (0.0f < Vector3DotProduct(forward, n)) continue;*/
				
				Texture2D t = textures[f];
				if (t.id) rlSetTexture(t.id); /* bind texture for this face */

				/* simple full-rect UVs: (0,0)-(1,1) ; flip V if needed to match texture orientation */
				const float 
					u0 = 0.0f, 
					v0 = 0.0f,
					u1 = 1.0f, 
					v1 = 1.0f;

				/* vertices are in world space around origin; map face index -> verts */
				Vector3 
					a = Vector3Transform(SkyBox_verts[SkyBox_faces[f][0]], rot),
					b = Vector3Transform(SkyBox_verts[SkyBox_faces[f][1]], rot),
					c = Vector3Transform(SkyBox_verts[SkyBox_faces[f][2]], rot),
					d = Vector3Transform(SkyBox_verts[SkyBox_faces[f][3]], rot);

				/* emit quad (inward-facing winding already chosen above) */
				rlTexCoord2f(u0, v0); rlVertex3f(a.x, a.y, a.z);
				rlTexCoord2f(u1, v0); rlVertex3f(b.x, b.y, b.z);
				rlTexCoord2f(u1, v1); rlVertex3f(c.x, c.y, c.z);
				rlTexCoord2f(u0, v1); rlVertex3f(d.x, d.y, d.z);

				rlEnd();
			}
			
		rlSetTexture(0); /* unbind (optional) */
	
    rlPopMatrix();
	
	rlEnableDepthMask();
	rlEnableDepthTest();
}


#endif /* SKYBOX_H */
