#ifndef SCENE_H
#define SCENE_H


#include "common.h"


// Map loading callback type
typedef bool (*MapLoaderCallback)(const char *filename, void **map_data, Entity **entities);

typedef struct Scene Scene;
typedef struct
MapType
{
    char             *name;
    
    void            (*Setup)(         Scene *scene, void    *map_data);
    void            (*Enter)(         Scene *scene);
    void            (*Update)(        Scene *scene, float    delta);
    CollisionResult (*CheckCollision)(Scene *scene, Entity  *entity, Vector3 to); 
    CollisionResult (*Raycast)(       Scene *scene, Vector3  from,   Vector3 to); 
    void            (*Render)(        Scene *scene, Head    *head);
    void            (*Exit)(          Scene *scene);
    void            (*Free)(          Scene *scene, void *map_data);
}
MapType;


Scene          *Scene_new( const MapType *scene_type, void *data, Engine *engine);
void            Scene_free(Scene *scene);

void            Scene_enter(         Scene *scene);
void            Scene_update(        Scene *scene, float    delta);
CollisionResult Scene_checkCollision(Scene *scene, Entity  *entity, Vector3 to);
CollisionResult Scene_raycast(       Scene *scene, Vector3  from,   Vector3 to);
void            Scene_render(        Scene *scene, Head    *head);
void            Scene_exit(          Scene *scene);


#endif /* SCENE_H */
