#include "kolibri.h"
#include "renderer.h"
#include "sectormap.h"

#include <GL/gl.h>
#include <cstddef>
#include <raylib.h>
#include <stdbool.h>


#define SECTOR_NONE SIZE_MAX
#define WALL_NONE   SIZE_MAX
#define VERTEX_NONE SIZE_MAX


typedef struct
{
    size_t    
        *entity_sectors, /* parallel to the scene entity list */
        *sector_parents;
    uint32    
        *sector_visited,
         frame;
    SectorMap  map;
}
SectorMapInternal;


/*
	Callback Forward Declarations
*/
void            SectorMapScene_setup(      Scene *scene, void    *map_data);
void            SectorMapScene_entityEnter(Scene *scene, Entity  *entity);
void            SectorMapScene_entityExit( Scene *scene, Entity  *entity);
void            SectorMapScene_render(     Scene *scene, Head    *head);
CollisionResult SectorMapScene_collision(  Scene *scene, Entity  *entity,  Vector3 to);
CollisionResult SectorMapScene_raycast(    Scene *scene, Vector3  from,    Vector3 to);
void            SectorMapScene_free(       Scene *scene);

/*
    VTable
*/
SceneVTable sectormap_Scene_Callbacks = {
    .Setup          = SectorMapScene_setup,
    .Enter          = NULL,
    .Update         = NULL,
    .EntityEnter    = SectorMapScene_entityEnter,
    .EntityExit     = SectorMapScene_entityExit,
    .CheckCollision = SectorMapScene_collision,
    .MoveEntity     = SectorMapScene_collision,
    .Raycast        = SectorMapScene_raycast,
    .PreRender      = NULL,
    .Render         = SectorMapScene_render,
    .Exit           = NULL,
    .Free           = SectorMapScene_free,
};


/**********************
    Private Methods
**********************/
static size_t
isSectorChild(SectorMap *map, size_t child_idx)
{
    Sector *child = &map->sectors[child_idx];

    for (
        size_t parent_idx = 0; 
        parent_idx < DynamicArray_length(map->sectors); 
        parent_idx++
    ) {
        if (parent_idx == child_idx) continue;

        Sector *parent    = &map->sectors[parent_idx];
        bool    all_match = true;

        for (
            size_t i = child->wall_start;
            i < child->wall_start + child->wall_count;
            i++
        ) {
            Wall *wall = &map->walls[i];

            /* Wall borders parent directly */
            if (wall->next_sector == parent_idx) continue;

            /* Check if wall lies on parent's boundary */
            bool on_parent_boundary = false;
            for (
                size_t j = parent->wall_start;
                j < parent->wall_start + parent->wall_count;
                j++
            ) {
                Wall *pwall = &map->walls[j];
                Vector2
                    pa = map->vertices[pwall->verts[0]],
                    pb = map->vertices[pwall->verts[1]],
                    ca = map->vertices[wall->verts[0]],
                    cb = map->vertices[wall->verts[1]],
                    intersection;

                if (CheckCollisionLines(pa, pb, ca, cb &intersection)) {
                    on_parent_boundary = true;
                    break;
                }
            }

            if (!on_parent_boundary) {
                all_match = false;
                break;
            }
        }

        /* Also check height differential */
        if (
            all_match
            && (child->floor_z < parent->floor_z
            || parent->ceiling_z < child->ceiling_z)
        ) {
            return parent_idx;
        }
    }

    return SECTOR_NONE;
}

static bool
pointInSector(SectorMap *map, Sector *sector, Vector2 point)
{
    size_t   crossings = 0;
    Wall    *walls     = map->walls;
    Vector2 *verts     = map->vertices;

    for (
        size_t i = sector->wall_start;
        i < sector->wall_start + sector->wall_count;
        i++
    ) {
        Wall *wall = &walls[i];
        /* Translate vertices relative to point */
        Vector2 
            a = {
                    verts[wall->verts[0]].x - point.x,
                    verts[wall->verts[0]].y - point.y,
                },
            b = {
                    verts[wall->verts[1]].x - point.x,
                    verts[wall->verts[1]].y - point.y,
                };

        /* Does this wall cross the +X axis? */
        if ((0.0f < a.y) != (0.0f < b.y)) {
            /* X intercept of the wall at y=0 */
            float x_intercept = a.x - a.y * (b.x - a.x) / (b.y - a.y);
            if (0.0f < x_intercept) {
                crossings++;
            }
        }
    }

    return (crossings % 2);
}

/* RENDERER PRIVATE METHODS */
static void
RenderWall(SectorMap *map, Wall *wall, Sector *sector)
{
    Vector2
        a2d = map->vertices[wall->verts[0]],
        b2d = map->vertices[wall->verts[1]];

    Vector3
        a_floor   = { a2d.x, sector->floor_z,   a2d.y },
        b_floor   = { b2d.x, sector->floor_z,   b2d.y },
        a_ceiling = { a2d.x, sector->ceiling_z, a2d.y },
        b_ceiling = { b2d.x, sector->ceiling_z, b2d.y };

    if (wall->next_sector == SECTOR_NONE) {
        /* Solid wall, full quad floor to ceiling */
        rlBegin(RL_QUADS);
            rlVertex3f(a_floor.x,   a_floor.y,   a_floor.z);
            rlVertex3f(b_floor.x,   b_floor.y,   b_floor.z);
            rlVertex3f(b_ceiling.x, b_ceiling.y, b_ceiling.z);
            rlVertex3f(a_ceiling.x, a_ceiling.y, a_ceiling.z);
        rlEnd();
    }
    else {
        /* Portal wall, upper and lower quads for height step */
        Sector *neighbor = &map->sectors[wall->next_sector];

        /* Upper quad, between neighbor ceiling and our ceiling */
        if (neighbor->ceiling_z < sector->ceiling_z) {
            Vector3
                a_neighbor_ceil = { a2d.x, neighbor->ceiling_z, a2d.y },
                b_neighbor_ceil = { b2d.x, neighbor->ceiling_z, b2d.y };

            rlBegin(RL_QUADS);
                rlVertex3f(a_neighbor_ceil.x, a_neighbor_ceil.y, a_neighbor_ceil.z);
                rlVertex3f(b_neighbor_ceil.x, b_neighbor_ceil.y, b_neighbor_ceil.z);
                rlVertex3f(b_ceiling.x,       b_ceiling.y,       b_ceiling.z);
                rlVertex3f(a_ceiling.x,       a_ceiling.y,       a_ceiling.z);
            rlEnd();
        }

        /* Lower quad, between our floor and neighbor floor */
        if (sector->floor_z < neighbor->floor_z) {
            Vector3
                a_neighbor_floor = { a2d.x, neighbor->floor_z, a2d.y },
                b_neighbor_floor = { b2d.x, neighbor->floor_z, b2d.y };

            rlBegin(RL_QUADS);
                rlVertex3f(a_floor.x,          a_floor.y,          a_floor.z);
                rlVertex3f(b_floor.x,          b_floor.y,          b_floor.z);
                rlVertex3f(b_neighbor_floor.x, b_neighbor_floor.y, b_neighbor_floor.z);
                rlVertex3f(a_neighbor_floor.x, a_neighbor_floor.y, a_neighbor_floor.z);
            rlEnd();
        }
    }
}

static void
RenderSectorFloorCeiling(SectorMapInternal *internal, size_t sector_idx)
{
    SectorMap *map      = &internal->map;
    Sector    *sector   = &map->sectors[sector_idx];
    bool       is_child = internal->sector_parents[sector_idx] != SECTOR_NONE;

    /* Flus rlgl batch before raw GL state changes */
    rlDrawRenderBatchActive();

    /* Write sector boundary into stencil buffer */
    glEnable(GL_STENCIL_TEST);
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 1, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    /* Draw wall loop as a fan to fill stencil region */
    Vector2 origin = map->vertices[map->walls[sector->wall_start].verts[0]];

    rlBegin(RL_TRIANGLES);
    for (
        size_t i = sector->wall_start + 1;
        i < sector->wall_start + sector->wall_count - 1;
        i++
    ) {
        Wall *wall = &map->walls[i];
        Vector2
            a = map->vertices[wall->verts[0]],
            b = map->vertices[wall->verts[1]];

        rlVertex3f(origin.x, sector->floor_z, origin.y);
        rlVertex3f(a.x,      sector->floor_z, a.y);
        rlVertex3f(b.x,      sector->floor_z, b.y);
    }
    rlEnd();

    rlDrawRenderBatchActive();

    /* Restore color and depth writing */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);

    /* Only draw where stencil == 1 */
    glStencilFunc(GL_NOTEQUAL, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    /* Child Sectors punch through parent depth */
    if (is_child) glDisable(GL_DEPTH_TEST);

    /* Draw floor and ceiling as large quads clipped by stencil */
    float s = 999999.0f;
    rlBegin(RL_QUADS);
        /* Floor */
        rlVertex3f(-s, sector->floor_z, -s);
        rlVertex3f( s, sector->floor_z, -s);
        rlVertex3f( s, sector->floor_z,  s);
        rlVertex3f(-s, sector->floor_z,  s);

        /* Ceiling -- reverse winding */
        rlVertex3f(-s, sector->ceiling_z,  s);
        rlVertex3f( s, sector->ceiling_z,  s);
        rlVertex3f( s, sector->ceiling_z, -s);
        rlVertex3f(-s, sector->ceiling_z, -s);
    rlEnd();

    rlDrawRenderBatchActive();

    if (is_child) glEnable(GL_DEPTH_TEST);

    /* Clear stencil region */
    glStencilMask(0xFF);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glDepthMask(GL_FALSE);

    rlBegin(RL_TRIANGLES);
    for (
        size_t i = sector->wall_start;
        i < sector->wall_start + sector->wall_count;
        i++
    ) {
        Wall *wall = &map->walls[i];
        Vector2
            a = map->vertices[wall->verts[0]];
            b = map->vertices[wall->verts[1]];

        rlVertex3f(origin.x, sector->floor_z, origin.y);
        rlVertex3f(a.x,      sector->floor_z, a.y);
        rlVertex3f(b.x,      sector->floor_z, b.y);
    }
    rlEnd();

    rlDrawRenderBatchActive();

    /* Restore all state */
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDisable(GL_STENCIL_TEST);
}


/*************************
    Callback functions
*************************/


/************
    SETUP
************/
void
SectorMapScene_setup(Scene *scene, void *map_data)
{
    SectorMapInternal *internal     = (SectorMapInternal*)map_data;
    size_t             sector_count = DynamicArray_length(internal->map.sectors);

    internal->entity_sectors = DynamicArray(size_t,   sector_count);
    internal->sector_parents = DynamicArray(size_t,   sector_count);
    internal->sector_visited = DynamicArray(uint32_t, sector_count);
    internal->frame          = 0;

    for (size_t i = 0; i < sector_count; i++) {
        DynamicArray_add(internal->sector_visited, 0);
        DynamicArray_add(internal->sector_parents, isSectorChild(&internal->map, i));
    }
}

/***********
    FREE
***********/
void
SectorMapScene_free(Scene *scene)
{
    SectorMapInternal *internal = Scene_getData(scene);
    DynamicArray_free(internal->entity_sectors);
    DynamicArray_free(internal->sector_parents);
    DynamicArray_free(internal->sector_visited);
}


void
SectorMapScene_entityEnter(Scene *scene, Entity *entity)
{
    (void)entity;
    SectorMapInternal *internal = Scene_getData(scene);
    DynamicArray_add(internal->entity_sectors, SIZE_MAX);
}


void
SectorMapScene_entityExit(Scene *scene, Entity *entity)
{
    (void)entity;
    SectorMapInternal  *internal  = Scene_getData(scene);
    Entity            **ent_list  = Scene_getEntities(scene);
    size_t              ent_count = Scene_getEntityCount(scene);

    for (size_t i = 0; i < ent_count; i++) {
        if (ent_list[i] == entity) {
            DynamicArray_delete(internal->entity_sectors, i, 1);
            return;
        }
    }
}


/****************
    COLLISION
****************/
CollisionResult
SectorMapScene_collision(Scene *scene, Entity *entity, Vector3 to)
{
    SectorMapInternal  *internal   = Scene_getData(scene);
    SectorMap          *map        = &internal->map;
    Entity            **ent_list   = Scene_getEntities(scene);
    size_t              ent_count  = Scene_getEntityCount(scene);
    Vector2             pos2d      = { to.x, to.z };
    Sector             *current    = NULL;
    size_t              sector_idx = SECTOR_NONE;

    /* Find entity index and its cached sector */
    for (size_t i = 0; i < ent_count; i++) {
        if (ent_list[i] == entity) {
            sector_idx = ((size_t*)internal->entity_sectors)[i];
            break;
        }
    }

    /* Check cached sector first */
    if (
        sector_idx != SECTOR_NONE 
        && pointInSector(map, &map->sectors[sector_idx], pos2d)
    ) {
        current = &map->sectors[sector_idx];
    }

    /* Check neighboring sectors via portals */
    if (!current && sector_idx != SECTOR_NONE) {
        Sector *cached = &map->sectors[sector_idx];
        for (
            size_t i = cached->wall_start;
            i < cached->wall_start + cached->wall_count;
            i++
        ) {
            Wall *wall = &map->walls[i];
            if (wall->next_sector == SECTOR_NONE) continue;
            if (pointInSector(map, &map->sectors[wall->next_sector], pos2d)) {
                current    = &map->sectors[wall->next_sector];
                sector_idx = wall->next_sector;
                break;
            }
        }
    }

    /* Fall back to full search */
    if (!current) {
        for (size_t i = 0; i < DynamicArray_length(map->sectors); i++) {
            if (pointInSector(map, &map->sectors[i], pos2d)) {
                current    = &map->sectors[i];
                sector_idx = i;
                break;
            }
        }
    }

    /* Update cached sector for this entity */
    for (size_t i = 0; i < ent_count; i++) {
        if (ent_list[i] == entity) {
            ((size_t*)internal->entity_sectors)[i] = sector_idx;
            break;
        }
    }
    
    /* Outside all sectors -- solid */
    if (!current) {
        return (CollisionResult) {
            .hit      = true,
            .position = entity->position,
            .normal   = V3_UP,
            .distance = 0.0f,
        };
    }

    /* Wall collision */
    Vector2
        from2d = { entity->position.x, entity->position.z },
        to2d   = { to.x,               to.z               };
    float   closest_t      = 1.0f;
    Vector2 closest_normal = { 0.0f, 0.0f };
    bool    wall_hit       = false;

    for (
        size_t i = current->wall_start;
        i < current->wall_start + current->wall_count;
        i++
    ) {
        Wall *wall = &map->walls[i];
        if (wall->next_sector != SECTOR_NONE) continue;

        Vector2
            a = map->vertices[wall->verts[0]],
            b = map->vertices[wall->verts[1]];

        /* Wall normal */
        Vector2 
            dir        = Vector2Subtract(b, a),
            normal     = Vector2Normalize((Vector2){-dir.y, dir.x}),
        /* Expand wall outward by entity radis */
            offset     = Vector2Scale(normal, entity->radius),
            a_expanded = Vector2Add(a, offset),
            b_expanded = Vector2Add(b, offset),
            intersection;

        /* Wall collision */
        if (
            CheckCollisionLines(
                    from2d, 
                    to2d, 
                    a_expanded, 
                    b_expanded, 
                    &intersection
                )
        ) {
            float wall_fraction = Vector2Length(Vector2Subtract(intersection, from2d))
                /     Vector2Length(Vector2Subtract(to2d,         from2d));
            if (wall_fraction < closest_t) {
                closest_t      = wall_fraction;
                wall_hit       = true;
                closest_normal = normal;
            }
        }

        /*  Corner collision - only check the first vertex, next wall checks
            the other
        */
        Vector2
            vertex = map->vertices[wall->verts[0]],
            move   = Vector2Subtract(to2d, from2d);
        float
            move_len_sq = Vector2LengthSqr(move),
            corner_proj = Vector2DotProduct(
                    Vector2Subtract(vertex, from2d), 
                    move
                ) / move_len_sq;
        corner_proj = CLAMP(corner_proj, 0.0f, 1.0f);
        Vector2 closest_point = Vector2Add(from2d, Vector2Scale(move, t));

        if (Vector2Distance(closest_point, vertex) <= entity->radius) {
            if (corner_proj < closest_t) {
                closest_t = corner_proj;
                wall_hit  = true;
                closest_normal = Vector2Normalize(
                        Vector2Subtract(closest_point, vertex)
                    );
            }
        }
    }

    if (wall_hit) {
        Vector3 hit_pos = Vector3Lerp(entity->position, to, closest_t);
        return (CollisionResult){
            .hit      = true,
            .position = hit_pos,
            .normal   = { closest_normal.x, 0.0f, closest_normal.y },
            .distance = Vector3Distance(entity->position, hit_pos),
        };
    }

    /* Floor collision */
    if (to.y <= current->floor_z) {
        return (CollisionResult) {
            .hit      = true,
            .position = { to.x, curent->floor_z, to.z },
            .normal   = V3_UP,
            .distance = fabsf(entity->position.y - current->floor_z),
        };
    }

    /* Ceiling Collision */
    if (current->ceiling_z <= to.y) {
        return (CollisionResult) {
            .hit      = true,
            .position = { to.x, current->ceiling_z, to.z },
            .normal   = { 0.0f, -1.0f, 0.0f },
            .distance = fabsf(entity->position.y - current->ceiling_z),
        };
    }

    return NO_COLLISION;
}


/**************
    RAYCAST
**************/
CollisionResult
SectorMapScene_raycast(Scene *scene, Vector3 from, Vector3 to)
{
    SectorMapInternal *internal     = Scene_getData(scene);
    SectorMap         *map          = &internal->map;
    Vector2            from2d       = { from.x, from.z };
    Vector2            to2d         = { to.x,   to.z   };
    CollisionResult    result       = NO_COLLISION;
    float              ray_len      = Vector3Distance(from, to);
    size_t             sector_count = DynamicArray_length(map->sectors);
    bool              *visited      = calloc(sector_count, sizeof(bool));

    /* Find starting sector */
    size_t start_sector = SIZE_MAX;
    for (size_t i = 0; i < map->sector_count; i++) {
        if (pointInSector(map, &map->sectors[i], from2d)) {
            start_sector = i;
            break;
        }
    }

    if (start_sector == SIZE_MAX) {
        free(visited);
        return NO_COLLISION;
    }

    /* Portal traversal queue */
    enum { MAX_QUEUE = 32 };
    struct {
        size_t sector_i;
        float  t_min,
               t_max;
    } queue[MAX_QUEUE];
    int head = 0,
        tail = 0;

    bool visited[map->sector_count];
    memset(visited, 0, sizeof(bool) * map->sector_count);

    queue[head++] = (typeof(*queue)){
        .sector_i = start_sector,
        .t_min    = 0.0f,
        .t_max    = 1.0f,
    };
    visited[start_sector] = true;

    float closest_t = 1.0f;

    while (head != tail) {
        typeof(*queue) current = queue[tail++];
        if (tail == MAX_QUEUE) tail = 0;

        Sector *sector = &map->sectors[current.sector_i];

        /* Check floor and ceiling */
        float ray_y_at_min = from.y + (to.y - from.y) * current.t_min;
        float ray_y_at_max = from.y + (to.y - from.y) * current.t_max;

        /* Floor intersection */
        if ((from.y > sector->floor_z) != (to.y > sector->floor_z)) {
            float floor_t = (sector->floor_z - from.y) / (to.y - from.y);
            if (floor_t < closest_t && floor_t >= current.t_min && floor_t <= current.t_max) {
                closest_t      = floor_t;
                Vector3 hit    = Vector3Lerp(from, to, floor_t);
                result = (CollisionResult){
                    .hit         = true,
                    .position    = hit,
                    .normal      = V3_UP,
                    .distance    = Vector3Distance(from, hit),
                    .material_id = 0,
                    .user_data   = NULL,
                    .entity      = NULL,
                };
            }
        }

        /* Ceiling intersection */
        if ((from.y > sector->ceiling_z) != (to.y > sector->ceiling_z)) {
            float ceil_t = (sector->ceiling_z - from.y) / (to.y - from.y);
            if (ceil_t < closest_t && ceil_t >= current.t_min && ceil_t <= current.t_max) {
                closest_t      = ceil_t;
                Vector3 hit    = Vector3Lerp(from, to, ceil_t);
                result = (CollisionResult){
                    .hit         = true,
                    .position    = hit,
                    .normal      = { 0.0f, -1.0f, 0.0f },
                    .distance    = Vector3Distance(from, hit),
                    .material_id = 0,
                    .user_data   = NULL,
                    .entity      = NULL,
                };
            }
        }

        /* Check walls */
        for (size_t i = sector->wall_start;
             i < sector->wall_start + sector->wall_count;
             i++)
        {
            Wall   *wall = &map->walls[i];
            Vector2 a    = map->vertices[wall->verts[0]];
            Vector2 b    = map->vertices[wall->verts[1]];

            Vector2 wall_intersection;
            if (!CheckCollisionLines(from2d, to2d, a, b, &wall_intersection)) continue;

            float wall_t = Vector2Length(Vector2Subtract(wall_intersection, from2d))
                         / Vector2Length(Vector2Subtract(to2d,             from2d));

            if (wall_t > closest_t || wall_t < current.t_min || wall_t > current.t_max) 
                continue;

            /* Check height at intersection point */
            float hit_y = from.y + (to.y - from.y) * wall_t;

            /* Portal wall -- check if ray passes through the opening */
            if (wall->next_sector != SIZE_MAX) {
                Sector *neighbor = &map->sectors[wall->next_sector];
                if (hit_y > neighbor->floor_z && hit_y < neighbor->ceiling_z
                    && !visited[wall->next_sector]
                    && head != (tail + MAX_QUEUE - 1) % MAX_QUEUE)
                {
                    visited[wall->next_sector] = true;
                    queue[head++] = (typeof(*queue)){
                        .sector_i = wall->next_sector,
                        .t_min    = wall_t,
                        .t_max    = current.t_max,
                    };
                    if (head == MAX_QUEUE) head = 0;
                }
                continue;
            }

            /* Solid wall -- check if ray hits within floor/ceiling */
            if (hit_y < sector->floor_z || hit_y > sector->ceiling_z) continue;

            closest_t      = wall_t;
            Vector3 hit    = Vector3Lerp(from, to, wall_t);
            Vector2 wall_dir    = Vector2Subtract(b, a);
            Vector2 wall_normal = Vector2Normalize((Vector2){ -wall_dir.y, wall_dir.x });

            result = (CollisionResult){
                .hit         = true,
                .position    = hit,
                .normal      = { wall_normal.x, 0.0f, wall_normal.y },
                .distance    = Vector3Distance(from, hit),
                .material_id = 0,
                .user_data   = NULL,
                .entity      = NULL,
            };
        }
    }

    free(visited);
    return result;
}

/*************
    RENDER
*************/
void
SectorMapScene_render(Scene *scene, Head *head)
{
    SectorMapInternal *internal     = Scene_getData(scene);
    SectorMap         *map          = &internal->map;
    Camera            *camera       = Head_getCamera(head);
    Renderer          *renderer     = Engine_getRenderer(Scene_getEngine(scene));
    Vector2            cam2d        = { camera->position.x, camera->position.z };
    size_t             sector_count = DynamicArray_length(map->sectors);

    internal->frame++;

    /* Find starting sector */
    size_t start_sector = SECTOR_NONE;
    for (size_t i = 0; i < DynamicArray_length(map->sectors); i++) {
        if (pointInSector(map, &map->sectors[i], cam2d)) {
            start_sector = i;
            break;
        }
    }

    if (start_sector == SECTOR_NONE) return;

    /* Portal traversal queue */
    enum { MAX_QUEUE = 32 };
    size_t queue[MAX_QUEUE];
    int
        head_q = 0,
        tail_q = 0;
    uint32 *sector_visited = internal->sector_visited;

    sector_visited[start_sector] = internal->frame;
    queue[head_q++] = start_sector;

    while (head_q != tail_q) {
        size_t sector_idx = queue[tail_q++];
        if (tail_q == MAX_QUEUE) tail_q = 0;

        Sector *sector = &map->sectors[sector_idx];

        /* Render floor and ceiling */
        RenderSectorFloorCeiling(internal, sector_idx);

        /* Render Walls */
        for (
            size_t i = sector->wall_start;
            i < sector->wall_start + sector->wall_count;
            i++
        ) {
            Wall *wall = &map->walls[i];

            /* Enqueue neighboring sector */
            if (
                wall->next_sector != SECTOR_NONE
                && sector_visited[wall->next_sector] != internal->frame
                && head_q != (tail_q + MAX_QUEUE - 1) % MAX_QUEUE
            ) {
                sector_visited[wall->next_sector] = internal->frame;
                queue[head_q++] = wall->next_sector;
                if (head_q == MAX_QUEUE) head_q = 0;
            }

            RenderWall(map, wall, sector);
        }
    }

    /* Render Entities */
    Entity **ent_list  = Scene_getEntities(scene);
    size_t   ent_count = Scene_getEntityCount(scene);
    for (size_t i = 0; i < ent_count; i++)
        Renderer_submitEntity(renderer, ent_list[i]);
}

/*********************
    PUBLIC METHODS
*********************/
Scene *
SectorMapScene_new(SectorMap *map, Engine *engine)
{
    SectorMapInternal internal = {
        .map            = *map,
        .entity_sectors = NULL,
        .sector_visited = NULL,
        .sector_parents = NULL,
        .frame          = 0,
    };

    return Scene_new(
        &sectormap_Scene_Callbacks,
        NULL,
        &internal,
        sizeof(SectorMapInternal),
        engine
    );
}

SectorMap *
SectorMap_makeTestLevel(void)
{
    SectorMap *map = malloc(sizeof(SectorMap));

    /* Vertices */
    map->vertices = DynamicArray(Vector2, 8);
    
    /* Outer room -- simple square */
    DynamicArray_add(map->vertices, ((Vector2){ -10.0f,  -10.0f }));
    DynamicArray_add(map->vertices, ((Vector2){  10.0f,  -10.0f }));
    DynamicArray_add(map->vertices, ((Vector2){  10.0f,   10.0f }));
    DynamicArray_add(map->vertices, ((Vector2){ -10.0f,   10.0f }));

    /* Inner room -- smaller square sharing a portal */
    DynamicArray_add(map->vertices, ((Vector2){  10.0f,  -5.0f  }));
    DynamicArray_add(map->vertices, ((Vector2){  20.0f,  -5.0f  }));
    DynamicArray_add(map->vertices, ((Vector2){  20.0f,   5.0f  }));
    DynamicArray_add(map->vertices, ((Vector2){  10.0f,   5.0f  }));

    /* Walls */
    map->walls = DynamicArray(Wall, 8);

    /* Outer room walls */
    DynamicArray_add(map->walls, ((Wall){ .verts = {0, 1}, .next_wall = SIZE_MAX, .next_sector = SIZE_MAX })); /* south */
    DynamicArray_add(map->walls, ((Wall){ .verts = {1, 2}, .next_wall = 4,        .next_sector = 1        })); /* east -- portal to inner room */
    DynamicArray_add(map->walls, ((Wall){ .verts = {2, 3}, .next_wall = SIZE_MAX, .next_sector = SIZE_MAX })); /* north */
    DynamicArray_add(map->walls, ((Wall){ .verts = {3, 0}, .next_wall = SIZE_MAX, .next_sector = SIZE_MAX })); /* west */

    /* Inner room walls */
    DynamicArray_add(map->walls, ((Wall){ .verts = {4, 5}, .next_wall = SIZE_MAX, .next_sector = SIZE_MAX })); /* east */
    DynamicArray_add(map->walls, ((Wall){ .verts = {5, 6}, .next_wall = SIZE_MAX, .next_sector = SIZE_MAX })); /* north */
    DynamicArray_add(map->walls, ((Wall){ .verts = {6, 7}, .next_wall = SIZE_MAX, .next_sector = SIZE_MAX })); /* west */
    DynamicArray_add(map->walls, ((Wall){ .verts = {7, 4}, .next_wall = 1,        .next_sector = 0        })); /* south -- portal back to outer room */

    /* Sectors */
    map->sectors = DynamicArray(Sector, 2);

    DynamicArray_add(map->sectors, ((Sector){
        .wall_start   = 0,
        .wall_count   = 4,
        .floor_z      = 0.0f,
        .ceiling_z    = 4.0f,
        .floor_light  = 1.0f,
        .ceiling_light = 1.0f,
    }));

    DynamicArray_add(map->sectors, ((Sector){
        .wall_start    = 4,
        .wall_count    = 4,
        .floor_z       = 0.0f,
        .ceiling_z     = 4.0f,
        .floor_light   = 1.0f,
        .ceiling_light = 1.0f,
    }));

    return map;
}
