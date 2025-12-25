#include "game.h"
#include "../explosion.h"
#include <raylib.h>


ExplosionInfo *explosion_Info;
Texture2D      explosion_atlas;


void
Game_mediaInit()
{
	char explosion_path[256];

	snprintf(explosion_path, sizeof(explosion_path), "%s%s", path_prefix,  "resources/sprites/explosion.png");

	explosion_atlas = LoadTexture(explosion_path);
	SetTextureFilter(explosion_atlas, TEXTURE_FILTER_BILINEAR);
	
	explosion_Info = ExplosionInfo_new(
			2.5f,
			0.5f,
			10.0f,
			2.0f,
			1.0f/12.0f,
			explosion_atlas,
			4, 4
		);
}
