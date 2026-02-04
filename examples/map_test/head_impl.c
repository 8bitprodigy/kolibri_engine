#include "game.h"
#include <raylib.h>


#define MOUSE_SENSITIVITY 0.05f
#define MOVE_SPEED 5.0f


/*
	Callback Forward Declarations
*/
void HeadPostRender(Head *head);
void HeadUpdate(    Head *head, float delta);

/*
	VTable definition
*/
HeadVTable head_Callbacks = {
		.Setup      = NULL,
		.Update     = HeadUpdate,
		.PreRender  = NULL,
		.PostRender = NULL,
		.Resize     = NULL,
		.Exit       = NULL,
		.Free       = NULL,
	};

/*
	Callback Definitions
*/

void
HeadPreRender(Head *head)
{
	ClearBackground(BLACK);
}

void
HeadPostRender(Head *head)
{
	//DrawFPS(10, 10);
}

void
HeadUpdate(Head *head, float delta)
{
	Camera      *camera = Head_getCamera(head);
	UpdateCamera(camera, CAMERA_FREE);
}
