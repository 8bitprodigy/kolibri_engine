/****************************************************************************************/
/*  Map.h                                                                               */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Module for loading a .map file.                                        */
/*                                                                                      */
/*  This source code is now public domain. For jurisdictions which do not recognize     */
/*  public domain, it's also available under 0BSD. See LICENSE file for details.        */
/*                                                                                      */
/****************************************************************************************/
#ifndef MAP_H
#define MAP_H

#include <math.h>
#include <raymath.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "BSP.h"
#include "Motion.h"

#define MAX_MAP_ENTITIES 4096
#define MAX_MAP_TEXINFO  8192
#define MAX_MAP_TEXTURES 1024

#pragma pack(1)

typedef struct
{
	int32 Version;
	char  TAG[4];
	int32 NumEntities;
} 
MAP_BrushFileHeader;

typedef struct
{
	int32 Flags;
	int32 NumFaces;
} 
MAP_BrushHeader;

typedef struct
{
	int32 Flags;
	float MipMapBias;
	float Alpha;					// Alpha for brush
	float FaceLight;				// Light intensity for face light
	float ReflectiveScale;
	char  TexName[32];
	float uVecX, uVecY, uVecZ;
	float DrawScaleX, OffsetX;
	float vVecX, vVecY, vVecZ;
	float DrawScaleY, OffsetY;

} 
MAP_FaceHeader;

#pragma pack()

typedef struct _MAP_Epair
{
	_MAP_Epair *Next;
	char       *Key;
	char       *Value;

} 
MAP_Epair;

#define ENTITY_HAS_ORIGIN		(1<<0)

typedef struct
{
	MAP_Brush *Brushes2;
/*
	geMotion  *Motion;			// Temp motion data for entity if it contains a model
*/
	MAP_Epair *Epairs;

	int32      ModelNum;			// If brushes != NULL, entity will have a model num

	// For light stage
	char       ClassName[64];
	geVec3d    Origin;				// Well, this is used everywhere...
	float      Angle;
	int32      Light;
	int32      LType;
	char       Target[32];
	char       TargetName[32];

	uint32     Flags;
} 
MAP_Entity;

extern	int32		NumEntities;
extern	MAP_Entity	Entities[MAX_MAP_ENTITIES];

//=====================================================================================
//	Entity parsing functions
//=====================================================================================
char		*ValueForKey(MAP_Entity *Ent, char *Key);
void		SetKeyValue(MAP_Entity *Ent, char *Key, char *Value);
float		FloatForKey(MAP_Entity *Ent, char *Key);
void		GetVectorForKey(MAP_Entity *Ent, char *Key, geVec3d *Vec);
geBoolean	GetVectorForKey2(MAP_Entity *Ent, char *Key, geVec3d *Vec);
void		GetColorForKey(MAP_Entity *Ent, char *Key, geVec3d *Vec);
char		*NewString(char *Str);

//=====================================================================================
//	Misc functions
//=====================================================================================
geBoolean	SaveEntityData(geVFile *VFile);
geBoolean	LoadEntityData(geVFile *VFile);
geBoolean	ConvertGFXEntDataToEntities(void);
geBoolean	ConvertEntitiesToGFXEntData(void);
void		FreeAllEntities(void);
geBoolean	LoadBrushFile(char *FileName);


#endif /* MAP_H */
