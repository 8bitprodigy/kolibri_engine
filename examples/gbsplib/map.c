/****************************************************************************************/
/*  map.c                                                                               */
/*  Ported from Genesis3D MAP.CPP to C99/raylib                                        */
/*                                                                                      */
/*  Only the functions needed by the BSP compiler are implemented.                      */
/*  File I/O (LoadBrushFile, SaveEntityData, etc.) is stubbed — we load geometry        */
/*  from the v220 map parser and drive the build via ProcessWorldModel directly.        */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include "map.h"


int32_t    NumEntities = 0;
MAP_Entity Entities[MAX_MAP_ENTITIES];

//=====================================================================================
//  NewString
//=====================================================================================
char *NewString(char *Str)
{
    char *New = malloc(strlen(Str) + 1);
    if (!New) { fprintf(stderr, "ERROR: NewString: out of memory\n"); return NULL; }
    strcpy(New, Str);
    return New;
}

//=====================================================================================
//  ValueForKey
//=====================================================================================
char *ValueForKey(MAP_Entity *Ent, char *Key)
{
    MAP_Epair *Ep;
    for (Ep = Ent->Epairs; Ep; Ep = Ep->Next)
        if (strcasecmp(Ep->Key, Key) == 0)
            return Ep->Value;
    return "";
}

//=====================================================================================
//  SetKeyValue
//=====================================================================================
void SetKeyValue(MAP_Entity *Ent, char *Key, char *Value)
{
    MAP_Epair *Ep;
    for (Ep = Ent->Epairs; Ep; Ep = Ep->Next)
    {
        if (strcasecmp(Ep->Key, Key) == 0)
        {
            free(Ep->Value);
            Ep->Value = NewString(Value);
            return;
        }
    }
    Ep        = calloc(1, sizeof(MAP_Epair));
    Ep->Next  = Ent->Epairs;
    Ent->Epairs = Ep;
    Ep->Key   = NewString(Key);
    Ep->Value = NewString(Value);
}

//=====================================================================================
//  GetVectorForKey2
//=====================================================================================
bool GetVectorForKey2(MAP_Entity *Ent, char *Key, Vector3 *Vec)
{
    char   *k = ValueForKey(Ent, Key);
    double  v1 = 0, v2 = 0, v3 = 0;

    Vec->x = Vec->y = Vec->z = 0.0f;

    if (!k || !k[0])
        return false;

    if (sscanf(k, "%lf %lf %lf", &v1, &v2, &v3) != 3)
        return false;

    Vec->x = (float)v1;
    Vec->y = (float)v2;
    Vec->z = (float)v3;
    return true;
}

//=====================================================================================
//  FreeAllEntities
//=====================================================================================
void FreeAllEntities(void)
{
    int32_t i;
    for (i = 0; i < NumEntities; i++)
    {
        MAP_Epair *Ep = Entities[i].Epairs;
        while (Ep)
        {
            MAP_Epair *Next = Ep->Next;
            free(Ep->Key);
            free(Ep->Value);
            free(Ep);
            Ep = Next;
        }
        /* Brush list is owned by the BSP build; do not free here */
        memset(&Entities[i], 0, sizeof(MAP_Entity));
    }
    NumEntities = 0;
}

/* ── Stubs for unused file-based pipeline ───────────────────────────────── */

bool LoadBrushFile(char *FileName)
{
    (void)FileName;
    fprintf(stderr, "STUB: LoadBrushFile — not implemented\n");
    return false;
}

bool ConvertEntitiesToGFXEntData(void)
{
    fprintf(stderr, "STUB: ConvertEntitiesToGFXEntData — not implemented\n");
    return false;
}
