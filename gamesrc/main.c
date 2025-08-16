#include "game.h"


Entity *player;
TestHeadData *head_data;

int
main(void)
{
	InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Kolibri Engine Test");
	
	Engine *engine = Engine_new(&engine_Callbacks);
	Head   *head   = Head_new(0, &head_Callbacks, engine);

	Head_setViewport(head, SCREEN_WIDTH, SCREEN_HEIGHT);
	RendererSettings *settings = Head_getRendererSettings(head);
//	settings->frustum_culling = false;
	Camera3D *cam = Head_getCamera(head);
	cam->fovy     = 45.0f;
	cam->up       = V3_UP;
	cam->position = (Vector3){0.0f, 1.75f, 0.0f};
	cam->target   = (Vector3){10.0f, 0.0f, 10.0f};

	Scene_new(&scene_Callbacks, NULL, engine);
	
	Entity *ents[21][21][2];
	int z = 0;
	for (int x = 0; x < 21; x++) {
		for (int y = 0; y < 21; y++) {
//			for (int z = 0; z < 21; z++) {
				ents[x][y][z]           = Entity_new(&entityTemplate, engine);
				ents[x][y][z]->visible  = true;
				ents[x][y][z]->active   = true;
				ents[x][y][z]->position = (Vector3){
					(x * 5.0f) - 50.0f,
					(z * 5.0f),
					(y * 5.0f) - 50.0f
				};
//			}
		}
	}

	player    = Entity_new(&playerTemplate, engine);
	head_data = (TestHeadData*)Head_getUserData(head);

	head_data->target     = player;
	head_data->eye_height = 1.75f;
	

	Engine_run(engine, 0);

	return 0;
}
