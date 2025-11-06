#ifndef RAYLIB_LOADERS_H
#define RAYLIB_LOADERS_H


typedef struct
{
	int 
		 font_size,
		*code_points,
		 code_point_count;
}
RaylibFontInfo;

/* Texture loaders */
void *raylib_textureLoader(  const char *path,  void *data);
void  raylib_textureReleaser(void       *asset, void *data);

/* Model loaders */
void *raylib_modelLoader(  const char *path,  void *data);
void  raylib_modelReleaser(void       *asset, void *data);

/* Sound loaders */
void *raylib_soundLoader(  const char *path,  void *data);
void  raylib_soundReleaser(void       *asset, void *data);

/* Music loaders */
void *raylib_musicLoader(  const char *path,  void *data);
void  raylib_musicReleaser(void       *asset, void *data);

/* Shader loaders */
void *raylib_shaderLoader(  const char *path,  void *data);
void  raylib_shaderReleaser(void       *asset, void *data);


	#ifdef RAYLIB_LOADERS_IMPLEMENTATION
		#include <stdlib.h>
		#include <raylib.h>


/* Texture loaders */
void *
raylib_textureLoader(const char *path, void *data)
{
	(void)data;
	Texture2D *texture = malloc(sizeof(Texture2D));
	if (!texture) return NULL;
	*texture = LoadTexture(path);
	return texture;
}

void 
raylib_textureReleaser(void *asset, void *data)
{
	(void)data;
	if (!asset) return;
	Texture2D *texture = (Texture2D *)asset;
	UnloadTexture(*texture);
	free(texture);
}

/* Model loaders */
void *
raylib_modelLoader(const char *path, void *data)
{
	(void)data;
	Model *model = malloc(sizeof(Model));
	if (!model) return NULL;
	*model = LoadModel(path);
	return model;
}

void 
raylib_modelReleaser(void *asset, void *data)
{
	(void)data;
	if (!asset) return;
	Model *model = (Model *)asset;
	UnloadModel(*model);
	free(model);
}

/* Sound loaders */
void *
raylib_soundLoader(const char *path, void *data)
{
	(void)data;
	Sound *sound = malloc(sizeof(Sound));
	if (!sound) return NULL;
	*sound = LoadSound(path);
	return sound;
}

void 
raylib_soundReleaser(void *asset, void *data)
{
	(void)data;
	if (!asset) return;
	Sound *sound = (Sound *)asset;
	UnloadSound(*sound);
	free(sound);
}

/* Music loaders */
void *
raylib_musicLoader(const char *path, void *data)
{
	(void)data;
	Music *music = malloc(sizeof(Music));
	if (!music) return NULL;
	*music = LoadMusicStream(path);
	return music;
}

void 
raylib_musicReleaser(void *asset, void *data)
{
	(void)data;
	if (!asset) return;
	Music *music = (Music *)asset;
	UnloadMusicStream(*music);
	free(music);
}

/* Shader loaders */
void *
raylib_shaderLoader(const char *path, void *data)
{
	(void)data;
	Shader *shader = malloc(sizeof(Shader));
	if (!shader) return NULL;
	*shader = LoadShader(NULL, path);  // Fragment shader only
	return shader;
}

void 
raylib_shaderReleaser(void *asset, void *data)
{
	(void)data;
	if (!asset) return;
	Shader *shader = (Shader *)asset;
	UnloadShader(*shader);
	free(shader);
}

/* Font loaders */
void *
raylib_fontLoader(const char *path, void *data)
{
	RaylibFontInfo *font_info = (RaylibFontInfo*)data;
	Font *font = malloc(sizeof(Font));
	if (!font) return NULL;
	*font = LoadFontEx(
			path, 
			font_info->font_size, 
			font_info->code_points, 
			font_info->code_point_count
		);
	return font;
}

void 
raylib_fontReleaser(void *asset, void *data)
{
	(void)data;
	if (!asset) return;
	Font *font = (Font *)asset;
	UnloadFont(*font);
	free(font);
}


	#endif /* RAYLIB_LOADERS_IMPLEMENTATION */
#endif /* RAYLIB_LOADERS_H */
