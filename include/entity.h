#ifndef ENTITY_H
#define ENTITY_H


enum {
    POSITION,
    ROTATION,
    SCALE,
    SKEW,
}

typedef void (*EntityCallback)(   Entity *entity);
typedef void (*EntityCallback_1f)(Entity *entity, float delta);

typedef struct 
Entity 
{
    union {
        Vector3 transform[4];
        struct {
            Vector3 
                position, 
                rotation,
                scale,
                skew;
        };
    };
    float           scale;
    Model          *model;
    Engine         *engine;
    void           *user_data; /* This is a void *, but could be replaced with a hashmap, perhaps */
    struct Entity  
                   *prev,
                   *next;
    void          (*update)(struct Entity* self, float dt);
    void          (*on_collision)(struct Entity* self, struct Entity* other);
    bool            visible;
} 
Entity;

void    Entity_addToScene(     Scene *scene, Entity *entity);
void    Entity_removeFromScene(Scene *scene, Entity *entity);
void    Entity_updateAll(      float  dt);
Entity *Entity_findByType(     int    type);

#endif /* ENTITY_H */
