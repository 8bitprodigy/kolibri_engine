#ifndef V220_MAP_PARSER_H
#define V220_MAP_PARSER_H

#include <raylib.h>
#include <stdbool.h>

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2

#define MAX_TOKEN_LENGTH 256
#define MAX_TEXTURE_NAME  64


/*
    AxisRemapping
        Describes how to transform map-space axes into engine-space axes,
        and optionally apply a uniform scale. All internal vectors (plane
        normals, texture axes, entity origins) are passed through this
        before being stored.
*/
typedef struct
{
    int   x_to;     /* Which engine axis receives the map's X  (0, 1, or 2) */
    int   y_to;     /* Which engine axis receives the map's Y  (0, 1, or 2) */
    int   z_to;     /* Which engine axis receives the map's Z  (0, 1, or 2) */
    int   flip_x;   /* 1 = negate X after remapping, 0 = keep               */
    int   flip_y;   /* 1 = negate Y after remapping, 0 = keep               */
    int   flip_z;   /* 1 = negate Z after remapping, 0 = keep               */
    float scale;    /* Uniform scale applied when RemapVector3 scale=true    */
}
AxisRemapping;

/* Preset configurations -------------------------------------------------- */

/* Quake / idTech (Z-up) — identity, no scale change */
static const AxisRemapping AXIS_REMAP_NONE =
    { 0, 1, 2,  0, 0, 0,  1.0f };

/* Raylib (Y-up) with 1/64 scale baked in — use for the render pass */
static const AxisRemapping AXIS_REMAP_RAYLIB =
    { 0, 2, 1,  0, 1, 0,  1.0f / 64.0f };

/* Raylib (Y-up) without scale — use for the BSP compile pass;
   apply MAP_SCALE yourself at render time                         */
static const AxisRemapping AXIS_REMAP_RAYLIB_NOSCALE =
    { 0, 2, 1,  0, 1, 0,  1.0f };


/* ----------------------------------------------------------------------- */

/*
    MapPlane
        One face of a brush as read from the .map file.
        Points have already been remapped through AxisRemapping before
        the plane equation is derived, so normal/distance are in
        engine space.
*/
typedef struct
{
    Vector3 normal;
    float   distance;
    char    texture[MAX_TEXTURE_NAME];
    Vector3 u_axis,   v_axis;
    float   u_offset, v_offset;
    float   rotation;
    float   u_scale,  v_scale;
}
MapPlane;

/*
    MapBrush
        Dynamic array of planes.  Free with FreeMapBrush().
*/
typedef struct
{
    MapPlane *planes;
    int       plane_count;
    int       plane_capacity;
}
MapBrush;

/*
    EntityKeyValue
        A single "key" "value" pair from an entity block.
*/
typedef struct
{
    char key  [64];
    char value[256];
}
EntityKeyValue;

/*
    MapEntity
        Dynamic arrays of key-value properties and child brushes.
        Free with FreeMapEntity().
*/
typedef struct
{
    EntityKeyValue *properties;
    int             property_count;
    int             property_capacity;

    MapBrush       *brushes;
    int             brush_count;
    int             brush_capacity;
}
MapEntity;

/*
    MapData
        Top-level parse result.
        world_brushes contains the brushes from the worldspawn entity
        (entity 0) promoted to the top level, matching the convention
        used by most BSP compilers.
        Free with FreeMapData().
*/
typedef struct
{
    MapEntity *entities;
    int        entity_count;
    int        entity_capacity;

    MapBrush  *world_brushes;
    int        world_brush_count;
    int        world_brush_capacity;
}
MapData;


/* ----------------------------------------------------------------------- */
/* Public API                                                               */
/* ----------------------------------------------------------------------- */

/*  Parse a Valve 220 .map file.
    remap  — axis remapping / scale to apply to all vectors.
    Returns a heap-allocated MapData on success, NULL on failure.
    Caller must free with FreeMapData(). */
MapData    *ParseValve220Map (const char *filename, AxisRemapping remap);

/*  Release all memory owned by a MapData returned by ParseValve220Map. */
void        FreeMapData      (MapData    *map);

/*  Convenience: look up a key in an entity's property list.
    Returns the value string, or NULL if the key is not present. */
const char *GetEntityProperty(MapEntity  *entity, const char *key);


#endif /* V220_MAP_PARSER_H */
