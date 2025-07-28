#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <string.h>

#include "_collision_.h"
#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "_spatial_hash_.h"
#include "common.h"


#define CELL_ALIGN( value ) ((int)floorf( (value) / CELL_SIZE))


typedef struct
CollisionScene
{
	SpatialHash *spatial_hash;
	EntityNode  *entities_ref; /* Points to engine's entity list */
	bool         needs_rebuild; /* Flag to rebuild hash next frame */
}
CollisionScene;


/* 
	Constructor
*/
CollisionScene *
CollisionScene__new(EntityNode *node)
{
	CollisionScene *scene = malloc(sizeof(CollisionScene));
	if (!scene) {
		ERR_OUT("Failed to allocate memory for CollisionScene.");
		return NULL;
	}

	scene->spatial_hash  = SpatialHash__new();
	scene->entities_ref  = node;
	scene->needs_rebuild = true;

	return scene;
}

/*
	Destructor
*/
void
CollisionScene__free(CollisionScene *scene)
{
	if (!scene) return;

	SpatialHash__free(scene->spatial_hash);
	free(scene);
}


/*
	Protected Methods
*/
void
CollisionScene__markRebuild(CollisionScene *scene)
{
	scene->needs_rebuild = true;
}

/* Insert entity into spatial hash */
void
CollisionScene__insertEntity(CollisionScene *scene, Entity *entity)
{
	if (!entity->physical) return;

	SpatialHash__insert(scene->spatial_hash, entity, entity->position, entity->bounds);
	scene->needs_rebuild = true;
}

void
CollisionScene__clear(CollisionScene *scene)
{
	SpatialHash__clear(scene);
}

/* Query entities in a region */
Entity **
CollisionScene__queryRegion(
	CollisionScene *scene, 
	Vector3 min, 
	Vector3 max, 
	int *count
)
{
	return SpatialHash__queryRegion(scene->spatial_hash, min, max, count);;
}


/* AABB Collision */
CollisionResult
Collision__checkAABB(Entity *a, Entity *b)
{
	CollisionResult result = {0};
	result.hit = false;

	if (
		   !(a->collision.masks & b->collision.layers)
		&& !(b->collision.masks & a->collision.layers)
	) {
		return result;
	}

	Vector3
		a_min = {
			a->position.x - a->bounds.x * 0.5f,
			a->position.y,
			a->position.z - a->bounds.z * 0.5f
		},
		a_max = {
			a->position.x + a->bounds.x * 0.5f,
			a->position.y + a->bounds.y,
			a->position.z + a->bounds.z * 0.5f
		},

		b_min = {
			b->position.x - b->bounds.x * 0.5f,
			b->position.y,
			b->position.z - b->bounds.z * 0.5f
		},
		b_max = {
			b->position.x + b->bounds.x * 0.5f,
			b->position.y + b->bounds.y,
			b->position.z + b->bounds.z * 0.5f
		};

	if (
		   (b_min.x <= a_max.x && a_min.x <= b_max.x)
		&& (b_min.y <= a_max.y && a_min.y <= b_max.y)
		&& (b_min.z <= a_max.z && a_min.z <= b_max.z)
	) {
		result.hit = true;
		result.entity = b;

		Vector3
			center_a = {
				(a_min.x + a_max.x) * 0.5f,
				(a_min.y + a_max.y) * 0.5f,
				(a_min.z + a_max.z) * 0.5f
			},
			center_b = {
				(b_min.x + b_max.x) * 0.5f,
				(b_min.y + b_max.y) * 0.5f,
				(b_min.z + b_max.z) * 0.5f
			},

			direction = Vector3Subtract(center_a, center_b);

		result.normal   = Vector3Normalize(direction);
		result.position = Vector3Lerp(center_a, center_b, 0.5f);
		result.distance = Vector3Distance(center_a, center_b);
	}

	
	return result;
}

/* Check collision for entity moving to new position */
CollisionResult
CollisionScene__checkCollision(CollisionScene *scene, Entity *entity, Vector3 to)
{
	CollisionResult result = {0};
	result.hit             = false;

	/* Temporary entity with new position for bounds checking */
	Entity temp_entity   = *entity;
	temp_entity.position = to;

	/* Get bounds for query */
	Vector3
		min_bounds = {
			to.x - entity->bounds.x * 0.5f,
			to.y,
			to.z - entity->bounds.z * 0.5f
		},
		max_bounds = {
			to.x + entity->bounds.x * 0.5f,
			to.y + entity->bounds.y,
			to.z + entity->bounds.z * 0.5f
		};

	/* Query spatial hash for potential collisions */
	int      candidate_count;
	Entity **candidates = SpatialHash__queryRegion(
		scene,
		min_bounds,
		max_bounds,
		&candidate_count
	);

	/* Check AABB collision with each candidate */
	for (int i = 0; i < candidate_count; i++) {
		Entity *other = candidates[i];
		if (other == entity) continue; /* Skip self */
		if (!other->physical) continue;

		CollisionResult test_result = Collision__checkAABB(&temp_entity, other);
		if (test_result.hit) {
			result = test_result;
			break; /* Return first collision found */
		}
	}

	EntityVTable *vtable = entity->vtable;
	if (result.hit && vtable && vtable->OnCollision) vtable->OnCollision(entity, &result);
	
	return result;
}


CollisionResult
Collision__checkRayAABB(Vector3 from, Vector3 to, Entity *entity)
{
	CollisionResult result = {0};
	result.hit = false;

	Vector3
		min_bounds = {
			entity->position.x - entity->bounds.x * 0.5f,
			entity->position.y,
			entity->position.z - entity->bounds.z * 0.5f
		},
		max_bounds = {
			entity->position.x + entity->bounds.x * 0.5f,
			entity->position.y + entity->bounds.y,
			entity->position.z + entity->bounds.z * 0.5f
		};

	/* Ray direction & length */
	Vector3 ray_dir = Vector3Subtract(to, from);
	float   ray_len = Vector3Length(ray_dir);

	if (ray_len < 0.0001f) return result; /* Zero-length ray */

	ray_dir = Vector3Normalize(ray_dir);

	float
		t_min = 0.0f,
		t_max = ray_len;

	for (int axis = 0; axis < 3; axis++) {
		float
			ray_origin    = (axis==0) ? from.x       : (axis==1) ? from.y       : from.z,
			ray_direction = (axis==0) ? ray_dir.x    : (axis==1) ? ray_dir.y    : ray_dir.z,
			box_min       = (axis==0) ? min_bounds.x : (axis==1) ? min_bounds.y : min_bounds.z,
			box_max       = (axis==0) ? max_bounds.x : (axis==1) ? max_bounds.y : max_bounds.z;

		if (fabsf(ray_direction) < 0.0001f) {
			/* Ray is parallel to this axis */
			if (ray_origin < box_min || box_max < ray_origin) {
				return result; /* Ray misses box */
			}
		}
		else {
			float
				t1 = (box_min - ray_origin) / ray_direction,
				t2 = (box_max - ray_origin) / ray_direction;

			/* Ensure t1 <= t2 */
			if (t2 < t1) {
				float temp = t1;
				t1 = t2;
				t2 = temp;
			}

			t_min = fmaxf(t_min, t1);
			t_max = fminf(t_max, t2);

			if (t_max < t_min) return result; /* No intersection */
		}
	}

	/* Intersection found */
	if (0.0f <= t_min && t_min <= ray_len) {
		result.hit      = true;
		result.distance = t_min;
		result.position = Vector3Add(from, Vector3Scale(ray_dir, t_min));
		result.entity   = entity;

		/* Calculate surface normal of the face hit */
		Vector3 
			hit_point = result.position,
			center    = {
				entity->position.x,
				entity->position.y + entity->bounds.y * 0.5f,
				entity->position.z
			},
			to_center = Vector3Subtract(hit_point, center);

		/* Find dominant axis for normal */
		float
			abs_x = fabsf(to_center.x),
			abs_y = fabsf(to_center.y),
			abs_z = fabsf(to_center.z);

		if (abs_y < abs_x && abs_z < abs_x) {
			result.normal = (Vector3){(0 < to_center.x) ? 1.0f : -1.0f, 0.0f, 0.0f};
		} else if (abs_z < abs_y) {
			result.normal = (Vector3){0.0f, (0 < to_center.y) ? 1.0f : -1.0f, 0.0f};
		} else {
			result.normal = (Vector3){0.0f, 0.0f, (0 < to_center.z) ? 1.0f : -1.0f};
		}
	}

	return result;
}


/* Simple raycast */
CollisionResult
Collision__raycast(CollisionScene *scene, Vector3 from, Vector3 to)
{
	CollisionResult closest_result = {0};
	closest_result.hit      = false;
	closest_result.distance = INFINITY;

	/* Get bounds along ray path */
	Vector3
		min_bounds = {
			fminf(from.x, to.x),
			fminf(from.y, to.y),
			fminf(from.z, to.z)
		},
		max_bounds = {
			fmaxf(from.x, to.x),
			fmaxf(from.y, to.y),
			fmaxf(from.z, to.z)
		};

	/* Query spatial hash */
	int      candidate_count;
	Entity **candidates = CollisionScene__queryRegion(
		scene, 
		min_bounds,
		max_bounds,
		&candidate_count
	);
	
	for (int i = 0; i < candidate_count; i++) {
		Entity *entity = candidates[i];

		/* Skip non-physical entities */
		if (!entity->physical) continue;

		CollisionResult result = Collision__checkRayAABB(from, to, entity);

		if (result.hit && result.distance < closest_result.distance) {
			closest_result = result;
		}
	}

	return closest_result;
}

/* System update - called each frame */
void
CollisionScene__update(CollisionScene *scene)
{
	if (!scene->needs_rebuild) return;

	SpatialHash__clear(scene->spatial_hash);

	if (!scene->entities_ref) return;

	EntityNode *current = scene->entities_ref;
	if (current) {
		do {
			Entity *entity = &current->base;

			if (entity->active && entity->physical) {
				CollisionScene__insertEntity(scene, entity);
			}

			current = current->next;
		} while (current != scene->entities_ref);
	}

	scene->needs_rebuild = false;
}
