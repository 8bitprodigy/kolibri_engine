#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <raylib.h>
#include <raymath.h>

#include "v220_map_parser.h"


/* =========================================================================
   Internal types
   ========================================================================= */

typedef struct
{
    char         *current;
    char          token[MAX_TOKEN_LENGTH];
    AxisRemapping remap;
}
Tokenizer;


/* =========================================================================
   Dynamic-array helpers
   ========================================================================= */

static bool
GrowArray(void **array, int *capacity, int count, size_t elem_size)
{
    if (count < *capacity)
        return true;

    int   new_cap = (*capacity == 0) ? 8 : (*capacity * 2);
    void *grown   = realloc(*array, (size_t)new_cap * elem_size);

    if (!grown)
        return false;

    *array    = grown;
    *capacity = new_cap;
    return true;
}


/* =========================================================================
   Axis remapping
   ========================================================================= */

static Vector3
RemapVector3(const Tokenizer *tok, Vector3 v, bool apply_scale)
{
    const AxisRemapping *r = &tok->remap;

    float c[3] = { v.x, v.y, v.z };

    float f[3];
    f[0] = c[0] * (r->flip_x ? -1.0f : 1.0f);
    f[1] = c[1] * (r->flip_y ? -1.0f : 1.0f);
    f[2] = c[2] * (r->flip_z ? -1.0f : 1.0f);

    Vector3 out = { f[r->x_to], f[r->y_to], f[r->z_to] };

    return Vector3Scale(out, apply_scale ? r->scale : 1.0f);
}


/* =========================================================================
   Tokenizer
   ========================================================================= */

static void
InitTokenizer(Tokenizer *tok, char *data, AxisRemapping remap)
{
    tok->current  = data;
    tok->token[0] = '\0';
    tok->remap    = remap;
}

static void
SkipWhitespaceAndComments(Tokenizer *tok)
{
    while (*tok->current)
    {
        if (*tok->current == ' '  || *tok->current == '\t' ||
            *tok->current == '\r' || *tok->current == '\n')
        {
            tok->current++;
        }
        else if (tok->current[0] == '/' && tok->current[1] == '/')
        {
            while (*tok->current && *tok->current != '\n')
                tok->current++;
        }
        else
        {
            break;
        }
    }
}

static bool
GetToken(Tokenizer *tok)
{
    SkipWhitespaceAndComments(tok);

    if (!*tok->current)
        return false;

    int len = 0;

    if (*tok->current == '"')
    {
        tok->current++;
        while (*tok->current && *tok->current != '"' &&
               len < MAX_TOKEN_LENGTH - 1)
        {
            tok->token[len++] = *tok->current++;
        }
        if (*tok->current == '"')
            tok->current++;
    }
    else
    {
        while (*tok->current &&
               *tok->current != ' '  && *tok->current != '\t' &&
               *tok->current != '\r' && *tok->current != '\n' &&
               len < MAX_TOKEN_LENGTH - 1)
        {
            tok->token[len++] = *tok->current++;
        }
    }

    tok->token[len] = '\0';
    return len > 0;
}

static void
UngetToken(Tokenizer *tok)
{
    tok->current -= strlen(tok->token);
}


/* =========================================================================
   Plane math
   ========================================================================= */

static void
CalculatePlane(Vector3 p1, Vector3 p2, Vector3 p3, MapPlane *plane)
{
    Vector3 v1 = Vector3Subtract(p2, p1);
    Vector3 v2 = Vector3Subtract(p3, p1);

    plane->normal   = Vector3Normalize(Vector3CrossProduct(v2, v1));
    plane->distance = Vector3DotProduct(plane->normal, p1);
}


/* =========================================================================
   Brush / plane parsing
   ========================================================================= */

static bool
ParseBrushPlane(Tokenizer *tok, MapPlane *plane)
{
    Vector3 p1, p2, p3;

#define EXPECT(str)   do { if (!GetToken(tok) || strcmp(tok->token, (str)) != 0) return false; } while (0)
#define NEXT_FLOAT(f) do { if (!GetToken(tok)) return false; (f) = (float)atof(tok->token); } while (0)

    EXPECT("("); NEXT_FLOAT(p1.x); NEXT_FLOAT(p1.y); NEXT_FLOAT(p1.z); EXPECT(")");
    EXPECT("("); NEXT_FLOAT(p2.x); NEXT_FLOAT(p2.y); NEXT_FLOAT(p2.z); EXPECT(")");
    EXPECT("("); NEXT_FLOAT(p3.x); NEXT_FLOAT(p3.y); NEXT_FLOAT(p3.z); EXPECT(")");

    p1 = RemapVector3(tok, p1, false);
    p2 = RemapVector3(tok, p2, false);
    p3 = RemapVector3(tok, p3, false);

    CalculatePlane(p1, p2, p3, plane);

    if (!GetToken(tok)) return false;
    strncpy(plane->texture, tok->token, MAX_TEXTURE_NAME - 1);
    plane->texture[MAX_TEXTURE_NAME - 1] = '\0';

    EXPECT("["); NEXT_FLOAT(plane->u_axis.x); NEXT_FLOAT(plane->u_axis.y);
                 NEXT_FLOAT(plane->u_axis.z); NEXT_FLOAT(plane->u_offset); EXPECT("]");
    plane->u_axis = RemapVector3(tok, plane->u_axis, false);

    EXPECT("["); NEXT_FLOAT(plane->v_axis.x); NEXT_FLOAT(plane->v_axis.y);
                 NEXT_FLOAT(plane->v_axis.z); NEXT_FLOAT(plane->v_offset); EXPECT("]");
    plane->v_axis = RemapVector3(tok, plane->v_axis, false);

    NEXT_FLOAT(plane->rotation);
    NEXT_FLOAT(plane->u_scale);
    NEXT_FLOAT(plane->v_scale);

#undef EXPECT
#undef NEXT_FLOAT

    return true;
}

static MapBrush *
ParseBrush(Tokenizer *tok)
{
    if (!GetToken(tok) || strcmp(tok->token, "{") != 0)
        return NULL;

    MapBrush *brush = (MapBrush *)calloc(1, sizeof(MapBrush));
    if (!brush) return NULL;

    while (GetToken(tok))
    {
        if (strcmp(tok->token, "}") == 0)
        {
            if (brush->plane_count == 0) { free(brush->planes); free(brush); return NULL; }
            return brush;
        }

        UngetToken(tok);

        if (!GrowArray((void **)&brush->planes, &brush->plane_capacity,
                        brush->plane_count, sizeof(MapPlane)))
        {
            fprintf(stderr, "v220_map_parser: out of memory growing plane array\n");
            free(brush->planes); free(brush); return NULL;
        }

        if (!ParseBrushPlane(tok, &brush->planes[brush->plane_count]))
        {
            fprintf(stderr, "v220_map_parser: error parsing brush plane %d\n", brush->plane_count);
            free(brush->planes); free(brush); return NULL;
        }

        brush->plane_count++;
    }

    fprintf(stderr, "v220_map_parser: unexpected EOF inside brush\n");
    free(brush->planes); free(brush);
    return NULL;
}


/* =========================================================================
   Entity parsing
   ========================================================================= */

static MapEntity *
ParseEntity(Tokenizer *tok)
{
    if (!GetToken(tok) || strcmp(tok->token, "{") != 0)
        return NULL;

    MapEntity *entity = (MapEntity *)calloc(1, sizeof(MapEntity));
    if (!entity) return NULL;

    while (GetToken(tok))
    {
        if (strcmp(tok->token, "}") == 0)
            return entity;

        if (strcmp(tok->token, "{") == 0)
        {
            UngetToken(tok);

            if (!GrowArray((void **)&entity->brushes, &entity->brush_capacity,
                            entity->brush_count, sizeof(MapBrush)))
            {
                fprintf(stderr, "v220_map_parser: out of memory growing brush array\n");
                goto fail;
            }

            MapBrush *b = ParseBrush(tok);
            if (!b) { fprintf(stderr, "v220_map_parser: error parsing entity brush %d\n", entity->brush_count); goto fail; }

            entity->brushes[entity->brush_count] = *b;
            free(b);
            entity->brush_count++;
        }
        else
        {
            if (!GrowArray((void **)&entity->properties, &entity->property_capacity,
                            entity->property_count, sizeof(EntityKeyValue)))
            {
                fprintf(stderr, "v220_map_parser: out of memory growing property array\n");
                goto fail;
            }

            EntityKeyValue *kv = &entity->properties[entity->property_count];

            strncpy(kv->key, tok->token, sizeof(kv->key) - 1);
            kv->key[sizeof(kv->key) - 1] = '\0';

            if (!GetToken(tok)) { fprintf(stderr, "v220_map_parser: EOF reading value for key '%s'\n", kv->key); goto fail; }

            if (strcmp(kv->key, "origin") == 0)
            {
                Vector3 origin;
                if (sscanf(tok->token, "%f %f %f", &origin.x, &origin.y, &origin.z) == 3)
                {
                    origin = RemapVector3(tok, origin, false);
                    snprintf(kv->value, sizeof(kv->value), "%f %f %f", origin.x, origin.y, origin.z);
                }
                else
                {
                    strncpy(kv->value, tok->token, sizeof(kv->value) - 1);
                    kv->value[sizeof(kv->value) - 1] = '\0';
                }
            }
            else
            {
                strncpy(kv->value, tok->token, sizeof(kv->value) - 1);
                kv->value[sizeof(kv->value) - 1] = '\0';
            }

            entity->property_count++;
        }
    }

    fprintf(stderr, "v220_map_parser: unexpected EOF inside entity\n");

fail:
    for (int i = 0; i < entity->brush_count; i++)
        free(entity->brushes[i].planes);
    free(entity->brushes);
    free(entity->properties);
    free(entity);
    return NULL;
}


/* =========================================================================
   Free helpers
   ========================================================================= */

static void
FreeMapBrushArray(MapBrush *brushes, int count)
{
    if (!brushes) return;
    for (int i = 0; i < count; i++)
        free(brushes[i].planes);
    free(brushes);
}

static void
FreeMapEntityArray(MapEntity *entities, int count)
{
    if (!entities) return;
    for (int i = 0; i < count; i++)
    {
        FreeMapBrushArray(entities[i].brushes, entities[i].brush_count);
        free(entities[i].properties);
    }
    free(entities);
}


/* =========================================================================
   Public API
   ========================================================================= */

MapData *
ParseValve220Map(const char *filename, AxisRemapping remap)
{
    char *file_data = LoadFileText(filename);
    if (!file_data)
    {
        fprintf(stderr, "v220_map_parser: could not load '%s'\n", filename);
        return NULL;
    }

    MapData *map = (MapData *)calloc(1, sizeof(MapData));
    if (!map) { UnloadFileText(file_data); return NULL; }

    Tokenizer tok;
    InitTokenizer(&tok, file_data, remap);

    while (GetToken(&tok))
    {
        if (strcmp(tok.token, "{") != 0)
            continue;

        UngetToken(&tok);

        if (!GrowArray((void **)&map->entities, &map->entity_capacity,
                        map->entity_count, sizeof(MapEntity)))
        {
            fprintf(stderr, "v220_map_parser: out of memory growing entity array\n");
            goto fail;
        }

        MapEntity *e = ParseEntity(&tok);
        if (!e) { fprintf(stderr, "v220_map_parser: error parsing entity %d\n", map->entity_count); goto fail; }

        map->entities[map->entity_count] = *e;
        free(e);
        map->entity_count++;

        if (map->entity_count == 1)
        {
            MapEntity *ws = &map->entities[0];

            for (int i = 0; i < ws->brush_count; i++)
            {
                if (!GrowArray((void **)&map->world_brushes, &map->world_brush_capacity,
                                map->world_brush_count, sizeof(MapBrush)))
                {
                    fprintf(stderr, "v220_map_parser: out of memory growing world_brushes\n");
                    goto fail;
                }
                map->world_brushes[map->world_brush_count++] = ws->brushes[i];
            }

            free(ws->brushes);
            ws->brushes        = NULL;
            ws->brush_count    = 0;
            ws->brush_capacity = 0;
        }
    }

    UnloadFileText(file_data);

    printf("v220_map_parser: loaded '%s' — %d entities, %d world brushes\n",
           filename, map->entity_count, map->world_brush_count);

    return map;

fail:
    UnloadFileText(file_data);
    FreeMapData(map);
    return NULL;
}


void
FreeMapData(MapData *map)
{
    if (!map) return;
    FreeMapEntityArray(map->entities,      map->entity_count);
    FreeMapBrushArray (map->world_brushes, map->world_brush_count);
    free(map);
}


const char *
GetEntityProperty(MapEntity *entity, const char *key)
{
    for (int i = 0; i < entity->property_count; i++)
        if (strcmp(entity->properties[i].key, key) == 0)
            return entity->properties[i].value;
    return NULL;
}
