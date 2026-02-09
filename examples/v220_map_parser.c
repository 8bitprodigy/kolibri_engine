#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <raylib.h>

#include "v220_map_parser.h"

#include <raymath.h>


static AxisRemapping g_remap;

static inline Vector3
RemapVector3(Vector3 v, bool scale)
{
    float components[3] = { v.x, v.y, v.z };
    float flipped[3];
    float result[3];
    
    // First apply flips to input components
    flipped[0] = components[0] * (g_remap.flip_x ? -1.0f : 1.0f);
    flipped[1] = components[1] * (g_remap.flip_y ? -1.0f : 1.0f);
    flipped[2] = components[2] * (g_remap.flip_z ? -1.0f : 1.0f);
    
    // Then remap to output axes
    result[0] = flipped[g_remap.x_to];
    result[1] = flipped[g_remap.y_to];
    result[2] = flipped[g_remap.z_to];
    
    return Vector3Scale(
        (Vector3){ result[0], result[1], result[2] },
        scale ? g_remap.scale : 1.0f
    );
}

/* Tokenizer state */
typedef struct 
{
    char *data;
    char *current;
    char token[MAX_TOKEN_LENGTH];
} 
Tokenizer;

/* Initialize tokenizer */
void 
InitTokenizer(Tokenizer *tok, char *data) 
{
    tok->data = data;
    tok->current = data;
    tok->token[0] = '\0';
}


// Skip whitespace and comments
void 
SkipWhitespace(Tokenizer *tok) 
{
    while (*tok->current) {
        if (*tok->current == ' ' || *tok->current == '\t' || 
            *tok->current == '\r' || *tok->current == '\n') {
            tok->current++;
        }
        else if (*tok->current == '/' && *(tok->current + 1) == '/') {
            // Skip line comment
            while (*tok->current && *tok->current != '\n') {
                tok->current++;
            }
        }
        else {
            break;
        }
    }
}

// Get next token
bool 
GetToken(Tokenizer *tok) 
{
    SkipWhitespace(tok);
    
    if (!*tok->current) return false;
    
    char *start = tok->current;
    int len = 0;
    
    // Handle quoted strings
    if (*tok->current == '"') {
        tok->current++; // Skip opening quote
        start = tok->current;
        
        while (*tok->current && *tok->current != '"' && len < MAX_TOKEN_LENGTH - 1) {
            tok->token[len++] = *tok->current++;
        }
        
        if (*tok->current == '"') tok->current++; // Skip closing quote
    }
    else {
        // Handle regular tokens
        while (*tok->current && *tok->current != ' ' && *tok->current != '\t' &&
               *tok->current != '\r' && *tok->current != '\n' && 
               len < MAX_TOKEN_LENGTH - 1) {
            tok->token[len++] = *tok->current++;
        }
    }
    
    tok->token[len] = '\0';
    return len > 0;
}

// Calculate plane from 3 points
void 
CalculatePlane(Vector3 p1, Vector3 p2, Vector3 p3, MapPlane *plane) 
{
    Vector3 v1 = Vector3Subtract(p2, p1);
    Vector3 v2 = Vector3Subtract(p3, p1);
    
    plane->normal = Vector3Normalize(Vector3CrossProduct(v2, v1));
    plane->distance = Vector3DotProduct(plane->normal, p1);
}

// Parse a brush plane line
bool 
ParseBrushPlane(Tokenizer *tok, MapPlane *plane) 
{
    Vector3 p1, p2, p3;
    
    // Parse first point ( x y z )
    if (!GetToken(tok) || strcmp(tok->token, "(") != 0) return false;
    if (!GetToken(tok)) return false; p1.x = atof(tok->token);
    if (!GetToken(tok)) return false; p1.y = atof(tok->token);
    if (!GetToken(tok)) return false; p1.z = atof(tok->token);
    if (!GetToken(tok) || strcmp(tok->token, ")") != 0) return false;
    
    // Parse second point ( x y z )
    if (!GetToken(tok) || strcmp(tok->token, "(") != 0) return false;
    if (!GetToken(tok)) return false; p2.x = atof(tok->token);
    if (!GetToken(tok)) return false; p2.y = atof(tok->token);
    if (!GetToken(tok)) return false; p2.z = atof(tok->token);
    if (!GetToken(tok) || strcmp(tok->token, ")") != 0) return false;
    
    // Parse third point ( x y z )
    if (!GetToken(tok) || strcmp(tok->token, "(") != 0) return false;
    if (!GetToken(tok)) return false; p3.x = atof(tok->token);
    if (!GetToken(tok)) return false; p3.y = atof(tok->token);
    if (!GetToken(tok)) return false; p3.z = atof(tok->token);
    if (!GetToken(tok) || strcmp(tok->token, ")") != 0) return false;

    // CRITICAL FIX: Remap points FIRST
    p1 = RemapVector3(p1, true);
    p2 = RemapVector3(p2, true);
    p3 = RemapVector3(p3, true);
    
    // THEN calculate plane from remapped points
    // This ensures normal matches the actual geometry
    CalculatePlane(p1, p2, p3, plane);
    
    // Parse texture name
    if (!GetToken(tok)) return false;
    strncpy(plane->texture, tok->token, sizeof(plane->texture) - 1);
    plane->texture[sizeof(plane->texture) - 1] = '\0';
    
    // Parse U axis [ ux uy uz offset ]
    if (!GetToken(tok) || strcmp(tok->token, "[") != 0) return false;
    if (!GetToken(tok)) return false; plane->u_axis.x = atof(tok->token);
    if (!GetToken(tok)) return false; plane->u_axis.y = atof(tok->token);
    if (!GetToken(tok)) return false; plane->u_axis.z = atof(tok->token);
    if (!GetToken(tok)) return false; plane->u_offset = atof(tok->token);
    if (!GetToken(tok) || strcmp(tok->token, "]") != 0) return false;

    plane->u_axis = RemapVector3(plane->u_axis, false);
    
    // Parse V axis [ vx vy vz offset ]
    if (!GetToken(tok) || strcmp(tok->token, "[") != 0) return false;
    if (!GetToken(tok)) return false; plane->v_axis.x = atof(tok->token);
    if (!GetToken(tok)) return false; plane->v_axis.y = atof(tok->token);
    if (!GetToken(tok)) return false; plane->v_axis.z = atof(tok->token);
    if (!GetToken(tok)) return false; plane->v_offset = atof(tok->token);
    if (!GetToken(tok) || strcmp(tok->token, "]") != 0) return false;

    plane->v_axis = RemapVector3(plane->v_axis, false);
    
    // Parse rotation, scale_x, scale_y
    if (!GetToken(tok)) return false; plane->rotation = atof(tok->token);
    if (!GetToken(tok)) return false; plane->u_scale = atof(tok->token);
    if (!GetToken(tok)) return false; plane->v_scale = atof(tok->token);
    
    return true;
}

// Parse a brush
bool 
ParseBrush(Tokenizer *tok, MapBrush *brush) 
{
    brush->plane_count = 0;
    
    if (!GetToken(tok) || strcmp(tok->token, "{") != 0) return false;
    
    while (GetToken(tok)) {
        if (strcmp(tok->token, "}") == 0) {
            return brush->plane_count > 0;
        }
        
        // Put back the token and parse plane
        tok->current -= strlen(tok->token);
        
        if (brush->plane_count >= MAX_BRUSH_PLANES) {
            printf("Warning: Brush has too many planes, skipping extras\n");
            continue;
        }
        
        if (!ParseBrushPlane(tok, &brush->planes[brush->plane_count])) {
            printf("Error parsing brush plane\n");
            return false;
        }
        
        brush->plane_count++;
    }
    
    return false; // No closing brace found
}

// Parse an entity
bool 
ParseEntity(Tokenizer *tok, MapEntity *entity) 
{
    entity->property_count = 0;
    entity->brush_count = 0;
    
    if (!GetToken(tok) || strcmp(tok->token, "{") != 0) return false;
    
    while (GetToken(tok)) {
        if (strcmp(tok->token, "}") == 0) {
            return true;
        }
        else if (strcmp(tok->token, "{") == 0) {
            // It's a brush - put back the token
            tok->current -= strlen(tok->token);
            
            if (entity->brush_count >= MAX_BRUSH_PLANES) {
                printf("Warning: Entity has too many brushes\n");
                continue;
            }
            
            if (!ParseBrush(tok, &entity->brushes[entity->brush_count])) {
                printf("Error parsing entity brush\n");
                return false;
            }
            entity->brush_count++;
        }
        else {
            // It's a key-value pair
            if (entity->property_count >= MAX_ENTITY_KEYS) {
                printf("Warning: Entity has too many properties\n");
                continue;
            }
            
            // Current token is the key
            strncpy(entity->properties[entity->property_count].key, tok->token, 
                   sizeof(entity->properties[entity->property_count].key) - 1);
            
            // Get the value
            if (!GetToken(tok)) return false;

            /* SPECIAL CASE: Remap origin vectors */
            if (
                strcmp(
                        entity->properties[entity->property_count].key, 
                        "origin"
                    ) == 0
            ) {
                Vector3 origin;
                if (
                    sscanf(
                            tok->token, 
                            "%f %f %f",
                            &origin.x, 
                            &origin.y, 
                            &origin.z
                        ) == 3
                ) {
                    origin = RemapVector3(origin, true); /* true = apply scale */
                    snprintf(
                            entity->properties[entity->property_count].value,
                            sizeof(entity->properties[entity->property_count].value),
                            "%f %f %f",
                            origin.x,
                            origin.y,
                            origin.z
                        );
                }
                else {
                    // Couldn't parse, store as-is
                    strncpy(entity->properties[entity->property_count].value, tok->token,
                        sizeof(entity->properties[entity->property_count].value) - 1);
                }
            }
            else {
                strncpy(entity->properties[entity->property_count].value, tok->token,
                    sizeof(entity->properties[entity->property_count].value) - 1);
            }
            
            entity->property_count++;
        }
    }
    
    return false; // No closing brace found
}

// Main parser function
MapData * 
ParseValve220Map(const char *filename, AxisRemapping remap) 
{
    g_remap = remap;
    
    // Load file
    char *file_data = LoadFileText(filename);
    if (!file_data) {
        printf("Error: Could not load map file %s\n", filename);
        return NULL;
    }
    
    MapData *map = (MapData*)malloc(sizeof(MapData));
    if (!map) {
        UnloadFileText(file_data);
        return NULL;
    }
    
    map->entity_count = 0;
    map->world_brush_count = 0;
    
    Tokenizer tok;
    InitTokenizer(&tok, file_data);
    
    // Parse entities
    while (GetToken(&tok)) {
        if (strcmp(tok.token, "{") == 0) {
            // Put back the token
            tok.current -= strlen(tok.token);
            
            if (map->entity_count >= MAX_ENTITIES) {
                printf("Warning: Too many entities in map\n");
                break;
            }
            
            if (!ParseEntity(&tok, &map->entities[map->entity_count])) {
                printf("Error parsing entity %d\n", map->entity_count);
                break;
            }
            
            // If it's worldspawn (first entity), move brushes to world_brushes
            if (map->entity_count == 0) {
                MapEntity *worldspawn = &map->entities[0];
                for (int i = 0; i < worldspawn->brush_count && map->world_brush_count < MAX_BRUSHES; i++) {
                    map->world_brushes[map->world_brush_count++] = worldspawn->brushes[i];
                }
                worldspawn->brush_count = 0; // Clear from entity
            }
            
            map->entity_count++;
        }
    }
    
    UnloadFileText(file_data);
    
    printf("Loaded map: %d entities, %d world brushes\n", 
           map->entity_count, map->world_brush_count);
    
    return map;
}

// Helper function to get entity property
const char * 
GetEntityProperty(MapEntity *entity, const char *key) 
{
    for (int i = 0; i < entity->property_count; i++) {
        if (strcmp(entity->properties[i].key, key) == 0) {
            return entity->properties[i].value;
        }
    }
    return NULL;
}

// Clean up
void 
FreeMapData(MapData *map) 
{
    if (map) {
        free(map);
    }
}

/*
// Example usage
int main(void) {
    MapData *map = ParseValve220Map("test.map");
    if (map) {
        printf("Successfully parsed map!\n");
        
        // Print some info
        for (int i = 0; i < map->entity_count; i++) {
            const char *classname = GetEntityProperty(&map->entities[i], "classname");
            if (classname) {
                printf("Entity %d: %s\n", i, classname);
            }
        }
        
        FreeMapData(map);
    }
    
    return 0;
}
*/
