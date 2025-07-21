#ifndef SCENE_H
#define SCENE_H

typedef struct 
{
    void   *map_data;           // Game casts this
    Entity *entity_ring;
    void   *persistent_data;    // Player data between scenes
} 
Scene;

// Map loading callback type
typedef bool (*MapLoaderCallback)(const char *filename, void **map_data, Entity **entities);

void Scene_init(              Scene      *scene);
void Scene_cleanup(           Scene      *scene);
void Scene_setActive(         Scene      *scene);
Scene *Scene_getActive(       void);
bool Scene_load(              const char *filename, Scene             *scene);
void Scene_registerMapLoader( const char *map_type, MapLoaderCallback  loader);
void Scene_setPersistentData( void       *data);
void *Scene_getPersistentData(void);

bool Scene_checkCollision(Scene *scene, Entity *entity);
void Scene_render(        Scene *scene, Head   *head);


#endif /* SCENE_H */
