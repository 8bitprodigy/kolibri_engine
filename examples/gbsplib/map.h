/****************************************************************************************/
/*  map.h                                                                               */
/*  Ported from Genesis3D MAP.H to C99/raylib                                          */
/****************************************************************************************/
#ifndef MAP_H
#define MAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <raylib.h>

#include "bsp.h"
#include "brush2.h"


/* Entity key/value helpers */
char    *ValueForKey(MAP_Entity *Ent, char *Key);
void     SetKeyValue(MAP_Entity *Ent, char *Key, char *Value);
char    *NewString(char *Str);
bool     GetVectorForKey2(MAP_Entity *Ent, char *Key, Vector3 *Vec);

/* Lifecycle */
void     FreeAllEntities(void);

/* File-based pipeline — not used when driving via ProcessWorldModel directly.
   Declared here so bsp.c compiles; implementations are stubs in map.c.      */
bool     LoadBrushFile(char *FileName);
bool     ConvertEntitiesToGFXEntData(void);

#endif /* MAP_H */
