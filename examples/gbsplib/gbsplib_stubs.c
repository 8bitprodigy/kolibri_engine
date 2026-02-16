/****************************************************************************************/
/*  gbsplib_stubs.c                                                                     */
/*                                                                                      */
/*  Stub definitions for Genesis3D symbols referenced by GBSPLib code paths we never   */
/*  invoke (the file-based pipeline: CreateBSP, UpdateEntities, CreateAreas, etc.).     */
/*                                                                                      */
/*  Entity functions (SetKeyValue, FreeAllEntities, Entities, NumEntities) now live     */
/*  in map.c. Only non-entity stubs remain here.                                        */
/*                                                                                      */
/****************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <raylib.h>
#include "bsp.h"
#include "map.h"

/* ── Cancellation flag ───────────────────────────────────────────────────── */
bool CancelRequest = false;

/* ── Texture info ────────────────────────────────────────────────────────── */
int32_t NumTexInfo = 0;

/* ── Sky data ────────────────────────────────────────────────────────────── */
typedef struct {
    int32_t Textures[6];
    Vector3 Axis;
    float   Dpm;
    float   DrawScale;
} GFX_SkyData_t;

GFX_SkyData_t GFXSkyData = { {-1,-1,-1,-1,-1,-1}, {0,0,0}, 0.0f, 1.0f };

/* ── GFX entity data ─────────────────────────────────────────────────────── */
void    *GFXEntData    = NULL;
int32_t  NumGFXEntData = 0;

/* ── Area portal system ──────────────────────────────────────────────────── */

GFX_Area       *GFXAreas         = NULL;
GFX_AreaPortal *GFXAreaPortals   = NULL;
int32_t         NumGFXAreas      = 0;
int32_t         NumGFXAreaPortals = 0;

/* ── File-based pipeline stubs ───────────────────────────────────────────── */
void BeginGBSPModels(void)
{
    fprintf(stderr, "STUB: BeginGBSPModels — not implemented\n");
}

bool LoadGBSPFile(const char *filename)
{
    (void)filename;
    fprintf(stderr, "STUB: LoadGBSPFile — not implemented\n");
    return false;
}

bool SaveGBSPFile(const char *filename)
{
    (void)filename;
    fprintf(stderr, "STUB: SaveGBSPFile — not implemented\n");
    return false;
}

void FreeGBSPFile(void)
{
    fprintf(stderr, "STUB: FreeGBSPFile — not implemented\n");
}
