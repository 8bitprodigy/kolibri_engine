/****************************************************************************************/
/*  Brush2.h                                                                            */
/*                                                                                      */
/*  Author: John Pollard                                                                */
/*  Description: Code to load, csg, and split brushes, etc...                           */
/*                                                                                      */
/*  The contents of this file are subject to the Genesis3D Public License               */
/*  Version 1.01 (the "License"); you may not use this file except in                   */
/*  compliance with the License. You may obtain a copy of the License at                */
/*  http://www.genesis3d.com                                                            */
/*                                                                                      */
/*  Software distributed under the License is distributed on an "AS IS"                 */
/*  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See                */
/*  the License for the specific language governing rights and limitations              */
/*  under the License.                                                                  */
/*                                                                                      */
/*  The Original Code is Genesis3D, released March 25, 1999.                            */
/*  Genesis3D Version 1.1 released November 15, 1999                                 */
/*  Copyright (C) 1999 WildTangent, Inc. All Rights Reserved           */
/*                                                                                      */
/****************************************************************************************/
#ifndef BRUSH2_H
#define BRUSH2_H


#include "mathlib.h"


#define	NUM_BRUSH_DEFAULT_SIDES		6

typedef struct MAP_Brush
{
	MAP_Brush	*Next;

	int32_t		EntityNum;
	int32_t		BrushNum;

	int32_t		Contents;

	Vector3		Mins, Maxs;

	int32_t		OrderID;

	int32_t		NumSides;
	GBSP_Side	OriginalSides[NUM_BRUSH_DEFAULT_SIDES];	
} MAP_Brush;

typedef struct GBSP_Brush
{
	GBSP_Brush	*Next;
	Vector3		Mins, Maxs;
	int32_t		Side, TestSide;	
	MAP_Brush	*Original;
	int32_t		NumSides;
	GBSP_Side	Sides[NUM_BRUSH_DEFAULT_SIDES];

} GBSP_Brush;

extern int32_t	gTotalBrushes;
extern int32_t	gPeekBrushes;

MAP_Brush	*AllocMapBrush(int32_t NumSides);
void		FreeMapBrush(MAP_Brush *Brush);
void		FreeMapBrushList(MAP_Brush *Brushes);
bool	MakeMapBrushPolys(MAP_Brush *ob);
MAP_Brush	*CopyMapBrush(MAP_Brush *Brush);
int32_t		CountMapBrushList(MAP_Brush *Brushes);

GBSP_Brush	*AllocBrush(int32_t NumSides);
void		FreeBrush(GBSP_Brush *Brush);
void		FreeBrushList(GBSP_Brush *Brushes);
void		ShowBrushHeap(void);
GBSP_Brush	*CopyBrush(GBSP_Brush *Brush);
void		BoundBrush(GBSP_Brush *Brush);
GBSP_Brush	*AddBrushListToTail(GBSP_Brush *List, GBSP_Brush *Tail);
int32_t		CountBrushList(GBSP_Brush *Brushes);
GBSP_Brush	*RemoveBrushList(GBSP_Brush *List, GBSP_Brush *Remove);
float		BrushVolume (GBSP_Brush *Brush);
void		CreateBrushPolys(GBSP_Brush *Brush);
GBSP_Brush	*BrushFromBounds (Vector3 *Mins, Vector3 *Maxs);
bool	CheckBrush(GBSP_Brush *Brush);
void		SplitBrush(GBSP_Brush *Brush, int32_t PNum, int32_t PSide, uint8_t MidFlags, bool Visible, GBSP_Brush **Front, GBSP_Brush **Back);
GBSP_Brush	*SubtractBrush(GBSP_Brush *a, GBSP_Brush *b);
GBSP_Brush	*CSGBrushes(GBSP_Brush *Head);

bool	OutputBrushes(char *FileName, GBSP_Brush *Brushes);
bool	OutputMapBrushes(char *FileName, MAP_Brush *Brushes);

#endif