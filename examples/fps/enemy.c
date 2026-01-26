#include <raylib.h>
#include <stdbool.h>

#include "dynamicarray.h"
#include "game.h"
#define THINKER_IMPLEMENTATION
#include "../thinker.h"
#include <raymath.h>



/*
	Callback Forward Declarations
*/
void enemySetup( Entity *entity);
void enemyUpdate(Entity *entity, float delta);
void enemyRender(Entity *entity, float delta);
void enemyFree(  Entity *entity);

/*
	AI state function forward declarations
*/
void enemy_ai_idle(        Entity *self, void *userdata);
void enemy_ai_run(         Entity *self, void *userdata);
void enemy_ai_chase(       Entity *self, void *userdata);
void enemy_ai_ranged_run(  Entity *self, void *userdata);
void enemy_ai_melee(       Entity *self, void *userdata);
void enemy_ai_shoot(       Entity *self, void *userdata);
void enemy_ai_pain(        Entity *self, void *userdata);
void enemy_ai_dead(        Entity *self, void *userdata);


/*
	Template Declarations
*/
Renderable         
	enemyRenderable_0 = {
			.data   = NULL,
			.Render = RenderModel,
		};

EntityVTable enemyCallbacks = {
		.Setup       = enemySetup,
		.Enter       = NULL,
		.Update      = enemyUpdate,
		.Render      = enemyRender,
		.OnCollision = NULL,
		.OnCollided  = NULL,
		.Teleport    = NULL,
		.Exit        = NULL,
		.Free        = enemyFree
	};

Entity enemyTemplate = {
		.renderables       = {NULL},
		.lod_distances     = {256.0f},
		.lod_count         = 1,
		.renderable_offset = {0.0f, 0.0f, 0.0f},
		.visibility_radius = 1.75f,
		.bounds            = {1.5f, 2.5f, 1.5f},
		.bounds_offset     = {0.0f, 1.25f, 0.0f},
		.floor_max_angle   = 45.0f,
		.max_slides        = 4,
		.user_data         = NULL,
		.vtable            = &enemyCallbacks,
		.position          = V3_ZERO_INIT,
		.orientation       = V4_ZERO_INIT,
		.scale             = V3_ONE_INIT,
		.collision         = {.layers = 1, .masks = 1},
		.active            = true,
		.visible           = true,
		.collision_shape   = COLLISION_BOX, 
		.solid             = true
	};


/*
	Callback Definitions
*/
void 
enemySetup(Entity *self)
{
}

void 
enemyUpdate(Entity *self, float delta)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	Vector3    
		*velocity  = &self->velocity,
		 vel       = *velocity;
		 
	bool is_on_floor = Entity_isOnFloor(self);
	
	if (!is_on_floor) {
		velocity->y -= (0.0f < velocity->y)
			? JUMP_GRAVITY * delta 
			: FALL_GRAVITY * delta;
	}
	
	Thinker_update(&data->thinker, self);
	Entity_moveAndSlide(self, Vector3Scale(self->velocity, delta));
}

void 
enemyRender(Entity *self, float delta)
{
	EnemyData *data   = (EnemyData*)&self->local_data;
	Engine    *engine = Entity_getEngine(self);
	
	float tick_elapsed_val = Engine_getTickElapsed(engine);
	self->renderable_offset = Vector3Lerp(
		data->prev_offset,
		V3_ZERO,
		tick_elapsed_val
	);

	AnimatedModel *anim_model = (AnimatedModel*)self->renderables[0]->data;

	if (0 <= self->current_anim && anim_model->animations) {
		ModelAnimation *anim = &anim_model->animations[self->current_anim];

		float
			anim_time = Entity_getAge(self),
			fps       = 25.0f;
		int total_frames = (int)(anim_time * fps);

		self->anim_frame = total_frames % anim->frameCount;
	}
}

void 
enemyFree(Entity *self)
{
}


/*
	AI Helper functions
*/
Entity *
Enemy_findTarget(Entity *self, float range)
{
	Scene       *scene      = Entity_getScene(self);
	BoundingBox  search_box = {
			.min = Vector3SubtractValue(self->position, range),
			.max = Vector3AddValue(     self->position, range)
		};

	Entity **nearby = Scene_queryRegion(scene, search_box);
	if (!nearby) {
		return NULL;
	}

	Entity *target = NULL;
	float closest_dist_sq = range * range;

	for (int i = 0; i < DynamicArray_length(nearby); i++) {
		Entity *candidate = nearby[i];

		if (candidate == self) {
			continue;
		}
		
		if (!(self->collision.masks & candidate->collision.layers)) {
			continue;
		}

		Vector3 to_target = Vector3Subtract(candidate->position, self->position);
		float   dist_sq   = Vector3LengthSqr(to_target);
		
		if (dist_sq < closest_dist_sq) {
			target          = candidate;
			closest_dist_sq = dist_sq;
		}
	}

	DynamicArray_free(nearby);
	return target;
}

bool
Enemy_canSeeTarget(Entity *self)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	if (!data->target || !data->target->active) {
		return false;
	}

	Scene *scene = Entity_getScene(self);

	Vector3 
		from = Vector3Add(self->position,         (Vector3){0.0f, 1.5f, 0.0f}),
		to   = Vector3Add(data->target->position, (Vector3){0.0f, 1.0f, 0.0f});
	        
	CollisionResult hit = Scene_raycast(scene, from, to, self);

	bool result = (!hit.hit || hit.entity == self || hit.entity == data->target);
	
	return result;
}

void
Enemy_facePoint(Entity *self, Vector3 point)
{
	Vector3 direction = Vector3Subtract(point, self->position);
	direction.y = 0;
	
	if (Vector3LengthSqr(direction) < 0.001f) return;

	direction         = Vector3Normalize(direction);
	float angle       = atan2f(direction.x, direction.z);
	self->orientation = QuaternionFromAxisAngle(V3_UP, angle);
}

void
Enemy_faceTarget(Entity *self)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	if (!data->target) return;

	Enemy_facePoint(self, data->target->position);
}

void
Enemy_faceRunDirection(Entity *self)
{
	EnemyData *data = (EnemyData*)&self->local_data;

	Enemy_facePoint(self, data->run_destination);
}

void
Enemy_moveToward(Entity *self, Vector3 target_pos, float speed)
{
	Vector3 direction = Vector3Subtract(target_pos, self->position);
	direction.y = 0.0f;

	float dist = sqrtf(direction.x * direction.x + direction.z * direction.z);

	if (dist < 0.001f) {
		self->velocity.x = self->velocity.z = 0.0f;
		return;
	}

	direction.x /= dist;
	direction.z /= dist;

	float actual_speed = speed;
	if (dist < 1.0f) {
		actual_speed = speed * dist;
	}

	self->velocity.x = direction.x * actual_speed;
	self->velocity.z = direction.z * actual_speed;
}

float
Enemy_distanceToTarget(Entity *self)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	if (!data->target) return INFINITY;

	return Vector3Distance(self->position, data->target->position);
}

void
Enemy_takeDamage(Entity *self, float damage, Entity *attacker)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	EnemyInfo *info = (EnemyInfo*)self->user_data;

	data->current_health -= damage;
	
	if (attacker && attacker != self) {
		data->target = attacker;
	}
	
	if (data->current_health <= 0) {
		Thinker_set(&data->thinker, enemy_ai_dead, 0.0f, NULL);
		self->solid = false;
		return;
	}

	/* React to pain */
	data->pain_time = Entity_getAge(self);
	Thinker_set(&data->thinker, enemy_ai_pain, 0.0f, NULL);
}

void
Enemy_melee(Entity *self) 
{
	EnemyData *data = (EnemyData*)&self->local_data;
	EnemyInfo *info = (EnemyInfo*)self->user_data;

	if (!data->target || info->melee_range <= 0.0f) return;
	
	if (Enemy_distanceToTarget(self) > info->melee_range) return;

	/* Simple line of sight attack */
	if (Enemy_canSeeTarget(self))
		Enemy_takeDamage(data->target, info->melee_damage, self);
}

void
Enemy_shoot(Entity *self)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	EnemyInfo *info = (EnemyInfo*)self->user_data;

	if (!data->target) {
		DBG_OUT("[Enemy_shoot]\tCouldn't shoot: No target set");
		return;
	}

	float dist = Enemy_distanceToTarget(self);
	if (dist < info->melee_range) {
		DBG_OUT("[Enemy_shoot]\tCouldn't shoot: Target in melee range");
		return;
	}

	/* Only shoot if we can see the target */
	if (!Enemy_canSeeTarget(self)) {
		DBG_OUT("[Enemy_shoot]\tCouldn't shoot: Target not visible");
		return;
	}

	Scene *scene = Entity_getScene(self);

	/* Calculate spawn position from enemy's "gun" */
	Vector3
		spawn_pos  = Vector3Add(
				self->position, 
				(Vector3){0.0f, 1.5f, 0.0f}
			),
		target_pos = Vector3Add(
				data->target->position, 
				(Vector3){0.0f, 1.0f, 0.0f}
			),
		direction  = Vector3Normalize(
				Vector3Subtract(
						target_pos, 
						spawn_pos
					)
				);

	Projectile_new(
		info->projectile_info,
		spawn_pos,
		direction,
		self,
		NULL,
		0,
		NULL
	);
}

void
Enemy_pickRunDestination(Entity *self)
{
	EnemyData *data = (EnemyData*)&self->local_data;

	float angle    = (rand() % 360) * DEG2RAD;
	float distance = 3.0f + (rand() % 100) * 0.05f; /* 3-8 units away */

	Vector3 offset = {
		cosf(angle) * distance,
		0.0f,
		sinf(angle) *distance
	};

	data->run_destination = Vector3Add(self->position, offset);
}


/*
	AI State Implementations
*/
void
enemy_ai_idle(Entity *self, void *userdata)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	EnemyInfo *info = (EnemyInfo*)self->user_data;

	if (self->current_anim != ENEMY_ANIM_IDLE) {
		self->current_anim = ENEMY_ANIM_IDLE;
		self->anim_frame   = 0;
	}

	self->velocity.x = self->velocity.z = 0.0f;
	DBG_OUT("enemy_ai_idle() entered.");

	/* Look for target */
	if (!data->target) {
		DBG_OUT("\tlooking for target.");
		data->target = Enemy_findTarget(self, info->sight_range);
	}

	/* Check if we can see current target */
	if (data->target) {
		
		bool can_see = Enemy_canSeeTarget(self);
		DBG_OUT("\tcan see target: %b", can_see);
		if (data->target->active && can_see) {
			/* Start chasing */
			DBG_OUT("\tgoing to chase...");
			Thinker_set(&data->thinker, enemy_ai_run, 0.5f, NULL);
			return;
		}
		else { /* Lost the target */
			DBG_OUT("\tlost the target.");
			data->target = NULL;
		}
	}
	Thinker_set(&data->thinker, enemy_ai_idle, 0.5f, NULL);
}

void
enemy_ai_run(Entity *self, void *userdata)
{
	EnemyData *data   = (EnemyData*)&self->local_data;
	EnemyInfo *info   = (EnemyInfo*)self->user_data;

	DBG_OUT("enemy_ai_run() entered.");
	if (self->current_anim != ENEMY_ANIM_WALK) {
		self->current_anim = ENEMY_ANIM_WALK;
		self->anim_frame   = 0;
	}
	
	/* Target lost */
	if (
		!data->target 
		|| !data->target->active 
		|| !Enemy_canSeeTarget(self)
	) {
		DBG_OUT("\tlost the target.");
		Thinker_set(&data->thinker, enemy_ai_idle, 0.0f, NULL);
		return;
	}

	if (info->projectile_info) {
		DBG_OUT("\tpicking run destination...");
		Enemy_pickRunDestination(self);
		Thinker_set(&data->thinker, enemy_ai_ranged_run, 0.0f, NULL);
		return;
	}
}

void
enemy_ai_ranged_run(Entity *self, void *userdata)
{
	EnemyData *data   = (EnemyData*)&self->local_data;
	EnemyInfo *info   = (EnemyInfo*)self->user_data;
	
	DBG_OUT("enemy_ai_ranged_run() entered.");
	if (self->current_anim != ENEMY_ANIM_IDLE) {
		self->current_anim = ENEMY_ANIM_IDLE;
		self->anim_frame   = 0;
	}
	
	if (
		!data->target 
		|| !data->target->active 
		|| !Enemy_canSeeTarget(self)
	) {
		DBG_OUT("\tlost the target.");
		Thinker_set(&data->thinker, enemy_ai_idle, 0.0f, NULL);
		return;
	}

	Vector3 to_dest    = Vector3Subtract(data->run_destination, self->position);
	float dist_to_dest = sqrtf(to_dest.x * to_dest.x + to_dest.z * to_dest.z);

	if (dist_to_dest < 1.5f) {
		/* Reached destination, switch to shooting */
		DBG_OUT("\tgoing to shoot...");
		self->velocity.x = self->velocity.z = 0.0f;
		data->next_attack_time = Entity_getAge(self);
		Thinker_set(&data->thinker, enemy_ai_shoot, 0.0f, NULL);
		return;
	}
	
	Enemy_faceRunDirection(self);
	Enemy_moveToward(self, data->run_destination, info->speed);
	Thinker_set(&data->thinker, enemy_ai_ranged_run, 0.1f, NULL);
}

void
enemy_ai_melee(Entity *self, void *userdata)
{
	EnemyData *data   = (EnemyData*)&self->local_data;
	EnemyInfo *info   = (EnemyInfo*)self->user_data;
	
	DBG_OUT("enemy_ai_melee() entered.");
	if (self->current_anim != ENEMY_ANIM_MELEE) {
		self->current_anim = ENEMY_ANIM_MELEE;
		self->anim_frame   = 0;
	}
	
	if (
		!data->target 
		|| !data->target->active 
		|| !Enemy_canSeeTarget(self)
	) {
		DBG_OUT("\tlost the target.");
		Thinker_set(&data->thinker, enemy_ai_idle, 0.0f, NULL);
		return;
	}

	float
		game_time = Entity_getAge(self),
		dist      = Enemy_distanceToTarget(self);

	Enemy_faceTarget(self);
	DBG_OUT("\tturning toward target target.");

	/* Melee if they get too close */
	if (0.0f < info->melee_range && dist < info->melee_range) {
		DBG_OUT("\tgoing to melee...");
		Enemy_melee(self);
		Thinker_set(&data->thinker, enemy_ai_melee, 0.5f, NULL);
		return;
	}

	/* Shoot if in range and cooldown is up */
	if (dist < info->projectile_range && game_time >= data->next_attack_time) {
		DBG_OUT("\meleeing!");
		//Enemy_shoot(self);
		data->next_attack_time = game_time + 0.5f + (rand() % 100) * 0.01f;
	}
	
	Thinker_set(&data->thinker, enemy_ai_shoot, 0.5f, NULL);
}

void
enemy_ai_shoot(Entity *self, void *userdata)
{
	EnemyData *data   = (EnemyData*)&self->local_data;
	EnemyInfo *info   = (EnemyInfo*)self->user_data;
	
	DBG_OUT("enemy_ai_shoot() entered.");
	if (self->current_anim != ENEMY_ANIM_SHOOT) {
		self->current_anim = ENEMY_ANIM_SHOOT;
		self->anim_frame   = 0;
	}
	
	if (
		!data->target 
		|| !data->target->active 
		|| !Enemy_canSeeTarget(self)
	) {
		DBG_OUT("\tlost the target.");
		Thinker_set(&data->thinker, enemy_ai_idle, 0.0f, NULL);
		return;
	}

	float
		game_time = Entity_getAge(self),
		dist      = Enemy_distanceToTarget(self);

	Enemy_faceTarget(self);
	DBG_OUT("\tturning toward target target.");

	/* Melee if they get too close */
	if (0.0f < info->melee_range && dist < info->melee_range) {
		DBG_OUT("\tgoing to melee...");
		Enemy_melee(self);
		Thinker_set(&data->thinker, enemy_ai_melee, 0.5f, NULL);
		return;
	}

	/* Shoot if in range and cooldown is up */
	if (dist < info->projectile_range && game_time >= data->next_attack_time) {
		DBG_OUT("\tshooting!");
		Enemy_shoot(self);
		data->next_attack_time = game_time + 0.8f + (rand() % 100) * 0.01f;
	}

	/* After shooting for a bit, pick a new position and run */
	if (data->next_attack_time + 2.0f <= game_time) {
		DBG_OUT("\tgoing to run...");
		Enemy_pickRunDestination(self);
		Thinker_set(&data->thinker, enemy_ai_ranged_run, 0.5f, NULL);
		return;
	}
	
	Thinker_set(&data->thinker, enemy_ai_shoot, 0.5f, NULL);
}

void
enemy_ai_chase(Entity *self, void *userdata)
{
	EnemyData *data   = (EnemyData*)&self->local_data;
	EnemyInfo *info   = (EnemyInfo*)self->user_data;
	
	DBG_OUT("enemy_ai_chase() entered.");
	if (self->current_anim != ENEMY_ANIM_WALK) {
		self->current_anim = ENEMY_ANIM_WALK;
		self->anim_frame   = 0;
	}
	
	if (
		!data->target 
		|| !data->target->active 
		|| !Enemy_canSeeTarget(self)
	) {
		Thinker_set(&data->thinker, enemy_ai_idle, 0.0f, NULL);
		return;
	}

	float dist = Enemy_distanceToTarget(self);

	Enemy_faceTarget(self);

	/* Melee when close */
	if (0.0f < info->melee_range && dist < info->melee_range) {
		self->velocity.x = self->velocity.z = 0.0f;
		Enemy_melee(self);
		Thinker_set(&data->thinker, enemy_ai_chase, 0.5f, NULL);
		return;
	}

	Enemy_moveToward(self, data->target->position, info->speed);
	Thinker_set(&data->thinker, enemy_ai_chase, 0.1f, NULL);
}

void
enemy_ai_pain(Entity *self, void *userdata)
{
	EnemyData *data = (EnemyData*)&self->local_data;
	
	if (self->current_anim != ENEMY_ANIM_PAIN) {
		self->current_anim = ENEMY_ANIM_PAIN;
		self->anim_frame   = 0;
	}

	/* Flinch */
	self->velocity.x = self->velocity.z = 0.0f;

	/* Resume chase */
	Thinker_set(&data->thinker, enemy_ai_run, 0.3f, NULL);
}

void
enemy_ai_dead(Entity *self, void *userdata)
{
	EnemyData *data = (EnemyData*)&self->local_data;

	self->velocity = V3_ZERO;
	self->solid    = false;

	/*
		Todo: The rest of the death state.
	*/
	Thinker_init(&data->thinker);
}


/*
	Public Methods
*/
Entity *
Enemy_new(
	EnemyInfo *info,
	Vector3    position,
	Engine     *engine
)
{
	Entity *enemy = Entity_new(
			&enemyTemplate, 
			engine, 
			sizeof(EnemyData)
		);
	if (!enemy) {
		ERR_OUT("Couldn't construct Enemy.");
		return NULL;
	}

	EnemyData *data  = (EnemyData*)&enemy->local_data;
	enemy->user_data    = info;
	enemy->position     = position;
	enemy->active       = true;
	enemy->visible      = true;
	enemy->current_anim = -1;  // Not initialized yet
	enemy->anim_frame = 0;

	enemy->lod_count = info->num_renderables;
	for (int i = 0; i < info->num_renderables; i++) {
		enemy->renderables[i]   = &info->renderables[i];
		enemy->lod_distances[i] = info->lod_distances[i];
	}
	
	Thinker_init(&data->thinker);
	data->prev_pos    = position;
	data->prev_offset = V3_ZERO;
	data->target      = NULL;
	
	Thinker_set(&data->thinker, enemy_ai_idle, 0.5f, NULL);

	return enemy;
}
