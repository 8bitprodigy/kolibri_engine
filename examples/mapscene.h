#ifndef MAPSCENE_H
#define MAPSCENE_H


extern SceneVTable MapScene_vtable;
typedef struct MapSceneData MapSceneData;

/*
	Constructor
*/
Scene *MapScene_new(const char *map_path, Engine *engine);


#endif /* MAPSCENE_H */
