#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdbool.h>
#include <string.h>

#include "_collision_.h"
#include "_engine_.h"
#include "_entity_.h"
#include "_head_.h"
#include "_spatialhash_.h"
#include "common.h"
#include "dynamicarray.h"
#define RAY2D_COLLISION_IMPLEMENTATION
#include "../examples/ray_collision_2d.h"


#define CELL_ALIGN( value ) ((int)floorf( (value) / CELL_SIZE))
#define COLLIDERS(a, b)     (((a) << 4) + (b))


typedef struct
CollisionScene
{
	SpatialHash *spatial_hash;
	Engine      *engine;
	Scene       *scene;
	bool         needs_rebuild; /* Flag to rebuild hash next frame */
}
CollisionScene;


/* 
	Constructor
*/
CollisionScene *
CollisionScene__new(Scene *scene)
{
	CollisionScene *col_scene = malloc(sizeof(CollisionScene));
	if (!scene) {
		ERR_OUT("Failed to allocate memory for CollisionScene.");
		return NULL;
	}

	col_scene->spatial_hash  = SpatialHash_new();
	col_scene->engine        = scene->engine;
	col_scene->scene         = scene;
	col_scene->needs_rebuild = true;

	return col_scene;
}

/*
	Destructor
*/
void
CollisionScene__free(CollisionScene *scene)
{
	if (!scene) return;

	SpatialHash_free(scene->spatial_hash);
	free(scene);
}


/*
	Protected Methods
*/
/* Insert entity into spatial hash */
void
CollisionScene__insertEntity(CollisionScene *scene, Entity *entity)
{
	if (!entity->collision_shape) return;

	SpatialHash_insert(scene->spatial_hash, entity, entity->position, entity->bounds);
	scene->needs_rebuild = true;
}

void
CollisionScene__clear(CollisionScene *scene)
{
	SpatialHash_clear((SpatialHash*)scene);
}

/* Query entities in a region */
Entity **
CollisionScene__queryRegion(
	CollisionScene *scene,
	BoundingBox bbox
)
{
	Entity **candidates = (Entity**)SpatialHash_queryRegion(
			scene->spatial_hash, 
			bbox
		);
	
	return candidates;
}

/* Cylinder Collision */
CollisionResult
Collision_checkCylinder(Entity *a, Entity *b)
{
	CollisionResult result = {0};
	result.hit = false;

	if (
		   !(a->collision.masks & b->collision.layers) 
		&& !(b->collision.masks & a->collision.layers)
	) {
		return result;
	}

	/* 2D circle collision for X/Z plane */
	float 
		dx          = a->position.x - b->position.x,
		dz          = a->position.z - b->position.z,
		distance_2d = sqrtf(dx * dx + dz * dz),
		radius_sum  = (a->bounds.x + b->bounds.x) * 0.5f,
	/* Y-axis (height overlap check */
		a_bottom = a->position.y,
		a_top    = a->position.y + a->bounds.y,
		b_bottom = b->position.y,
		b_top    = b->position.y + b->bounds.y;

	if (
		   distance_2d <= radius_sum 
		&& a_bottom    <= b_top 
		&& b_bottom    <= a_top
	) {
		result.hit      = true;
		result.entity   = b;
		result.distance = distance_2d;
		result.position = (Vector3){
			(a->position.x + b->position.x) * 0.5f,
			(a->position.y + b->position.y) * 0.5f,
			(a->position.z + b->position.z) * 0.5f
		};

		/* Normal points from b to a */
		if (0.0001f < distance_2d) {
			result.normal = (Vector3){dx / distance_2d, 0.0f, dz / distance_2d};
		}
		else {
			result.normal = (Vector3){1.0f, 0.0f, 0.0f}; /* fallback */
		}
	}
	
	return result;
}

/* AABB Collision */
CollisionResult
Collision_checkAABB(Entity *a, Entity *b)
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

CollisionResult
Collision_checkSphere(Entity *a, Entity *b)
{
	CollisionResult result = {0};
	result.hit             = false;

	Vector3
		a_pos = a->position,
		b_pos = b->position;

	float distance = Vector3Distance(a_pos, b_pos);
	if (distance < (a_pos.x + b_pos.x)) return result;

	result.distance = distance;
	result.normal   = Vector3Normalize(Vector3Subtract(a_pos, b_pos));
	result.position = Vector3Add(b_pos, Vector3Scale(result.normal, b->bounds.x));
	
	return result;
}

CollisionResult
Collision_checkMixed(Entity *aabb_entity, Entity *cyl_entity, bool cyl_is_b)
{
    CollisionResult result = {0};
    result.hit = false;

	/* Find closest point on AABB to Cylinder center in X/Z plane */
	float
		cyl_x = cyl_entity->position.x,
		cyl_z = cyl_entity->position.z,
		aabb_min_x = aabb_entity->position.x - aabb_entity->bounds.x * 0.5f,
		aabb_max_x = aabb_entity->position.x + aabb_entity->bounds.x * 0.5f,
		aabb_min_z = aabb_entity->position.z - aabb_entity->bounds.z * 0.5f,
		aabb_max_z = aabb_entity->position.z + aabb_entity->bounds.z * 0.5f,

		closest_x = CLAMP(cyl_x, aabb_min_x, aabb_max_x),
		closest_z = CLAMP(cyl_z, aabb_min_z, aabb_max_z),

		dx              = cyl_x - closest_x,
		dz              = cyl_z - closest_z,
		distance        = sqrtf(dx * dx + dz * dz),
		cylinder_radius = cyl_entity->bounds.x * 0.5f,
		
	/* Y-axis overlap */
		aabb_bottom = aabb_entity->position.y,
		aabb_top    = aabb_entity->position.y + aabb_entity->bounds.y,
		cyl_bottom  = cyl_entity->position.y,
		cyl_top     = cyl_entity->position.y + cyl_entity->bounds.y;

	if (
		   distance    <= cylinder_radius
		&& aabb_bottom <= cyl_top
		&& cyl_bottom  <= aabb_top
	) {
		result.hit      = true;
		result.entity   = cyl_is_b ? cyl_entity : aabb_entity;
		result.distance = distance;
		result.position = (Vector3){
				closest_x, 
				(aabb_bottom + cyl_bottom) * 0.5f, 
				closest_z
			};

		if (0.0001f < distance) {
			float normal_sign = cyl_is_b ? 1.0f : -1.0f;
			result.normal = (Vector3){
					dx * normal_sign / distance, 
					0.0f, 
					dz * normal_sign / distance
				};
		}
		else {
			result.normal = (Vector3){cyl_is_b ? 1.0f : -1.0f, 0.0f, 0.0f};
		}
	}
    
	return result;
}

CollisionResult
Collision_checkDiscreet(Entity *a, Entity *b)
{
	if (
		   !(a->collision.masks & b->collision.layers)
		&& !(b->collision.masks & a->collision.layers)
	) {
		return NO_COLLISION;
	}

	uint8 colliders = COLLIDERS(a->collision_shape, b->collision_shape);

	switch (colliders) {
	case COLLIDERS(COLLISION_BOX,      COLLISION_BOX):
		return Collision_checkAABB(a, b);
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_CYLINDER):
		return Collision_checkMixed(a, b, true);
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_CYLINDER):
		return Collision_checkCylinder(a, b);
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_BOX):
		return Collision_checkMixed(b, a, false);
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_SPHERE):
		return Collision_checkSphere(a, b);
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_BOX): /* FALLTHROUGH */
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_CYLINDER):
	default:
		break;
	}
	
	return NO_COLLISION;
}

/* Check collision for entity moving to new position */
CollisionResult
CollisionScene__checkCollision(
	CollisionScene *scene, 
	Entity         *entity, 
	Vector3         to
)
{
	CollisionResult result = {0};
	result.hit             = false;

	if (!entity->collision_shape) return result;
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
	Entity **candidates = CollisionScene__queryRegion(
		scene,
		(BoundingBox){min_bounds, max_bounds}
	);

	/* Check AABB collision with each candidate */
	for (int i = 0; i < DynamicArray_length(candidates); i++) {
		Entity *other = candidates[i];
		if (other == entity) continue; /* Skip self */
		if (!other->collision_shape) continue;

		CollisionResult test_result = Collision_checkDiscreet(
				&temp_entity, 
				other
			);
		if (test_result.hit) {
			result = test_result;
			break; /* Return first collision found */
		}
	}

	DynamicArray_free(candidates);
	return result;
}


/*************************************
	CONTINUOUS COLLISION DETECTION
*************************************/
/* CCD: Cylinder-Cylinder */
CollisionResult
Collision_checkContinuousCylinder(Entity *a, Entity *b, Vector3 movement)
{
	CollisionResult result = {0};
	result.hit             = false;
	result.distance        = Vector3Length(movement);

	Vector3
		from          = a->position,
		to            = Vector3Add(from, movement);
	float move_length = Vector3Length(movement);

	if (move_length < 0.0001f) {
		Entity temp_a = *a;
		temp_a.position = to;
		return Collision_checkCylinder(&temp_a, b);
	}

	/* 2D collision */
	Vector2
		center_a    = {from.x, from.z},
		center_b    = {b->position.x, b->position.z},
		movement_2d = {movement.x, movement.z},
		intersection;
	float
		radius_a = a->bounds.x * 0.5f,
		radius_b = b->bounds.x * 0.5f;

	result.hit = CheckCollisionRay2dCircle(
			(Ray2d) {
					center_a,
					movement_2d
				},
			center_b,
			radius_a + radius_b,
			&intersection
		);

	if (!result.hit) return result;
	
	/* Check Y-axis overlap at collision time */
	float
		col_dist    = Vector2Length(Vector2Subtract(intersection, center_a)),
		t_collision = result.distance / move_length,
		collision_y = from.y + movement.y * t_collision,
		a_bottom    = collision_y,
		a_top       = collision_y + a->bounds.y,
		b_bottom    = b->position.y,
		b_top       = b->position.y + b->bounds.y;

	/* No Y-axis overlap */
	if (b_top < a_bottom || a_top < b_bottom) {
		result.hit = false;
		return result;
	}

	/* Collision found */
	result.entity   = b;
	result.distance = col_dist;
	result.position = Vector3Add(from, Vector3Scale(movement, t_collision));
	
	return result;
}

/* CCD: AABB - AABB */
CollisionResult
Collision_checkContinuousAABB(Entity *a, Entity *b, Vector3 movement)
{
    CollisionResult result = {0};
    result.hit             = false;
    result.distance        = Vector3Length(movement);

    Vector3 from = a->position;
    float move_length = Vector3Length(movement);

    if (move_length < 0.0001f) {
        Entity temp_a   = *a;
        temp_a.position = Vector3Add(from, movement);
        return Collision_checkAABB(&temp_a, b);
    }

    /* Add A and B's AABBs together */
    Vector3 
		*b_pos    = &b->position,
		*b_bounds = &b->bounds,
		*a_bounds = &a->bounds;
    
    BoundingBox expanded_box = {
        {
            b_pos->x - b_bounds->x * 0.5f - a_bounds->x * 0.5f,
            b_pos->y - b_bounds->y * 0.5f - a_bounds->y * 0.5f,
            b_pos->z - b_bounds->z * 0.5f - a_bounds->z * 0.5f
        },
        {
            b_pos->x + b_bounds->x * 0.5f + a_bounds->x * 0.5f,
            b_pos->y + b_bounds->y * 0.5f + a_bounds->y * 0.5f,
            b_pos->z + b_bounds->z * 0.5f + a_bounds->z * 0.5f
        }
    };
    
    Ray ray = { from, Vector3Normalize(movement) };
    RayCollision collision = GetRayCollisionBox(ray, expanded_box);
    
    /* Check if collision is within our movement distance and in forward direction */
    if (collision.hit && 
        collision.distance >= 0.0f && 
        collision.distance <= move_length) {
        
        result.hit      = true;
        result.distance = collision.distance;
        result.entity   = b;
        result.position = collision.point;
        result.normal   = collision.normal;
    }
    
    return result;
}

CollisionResult
Collision_checkContinuousSphere(
	Entity  *sphere_1,
	Entity  *sphere_2,
	Vector3  movement
)
{
    CollisionResult result = {0};
    result.hit             = false;
    result.distance        = Vector3Length(movement);

    float   radius   = sphere_1->radius + sphere_2->radius;
    Vector3 position = Vector3Add(
			sphere_2->position, 
			sphere_2->bounds_offset
		);
    Ray     ray      = {sphere_1->position, Vector3Normalize(movement)};
     
	RayCollision collision = GetRayCollisionSphere(ray, position, radius);

	/* Check if collision is within our movement distance and in forward direction */
    if (collision.hit && 
        collision.distance >= 0.0f && 
        collision.distance <= result.distance) {
        
        result.hit      = true;
        result.distance = collision.distance;
        result.entity   = sphere_2;
        result.position = collision.point;
        result.normal   = collision.normal;
    }

    return result;
}

/* CCD: AABB - Cylinder using Minkowski sum approach */
CollisionResult
Collision_checkContinuousAABBCylinder(
	Entity  *aabb, 
	Entity  *cylinder, 
	Vector3  movement, 
	bool     aabb_is_moving
)
/*
	Currently broken.
*/
{
    CollisionResult result = {0};
    result.hit = false;
    result.distance = Vector3Length(movement);
	
    Entity *moving_entity = aabb_is_moving ? aabb : cylinder;
    Entity *static_entity = aabb_is_moving ? cylinder : aabb;
    
    if (!(moving_entity->collision.masks & static_entity->collision.layers) &&
        !(static_entity->collision.masks & moving_entity->collision.layers)) {
        return result;
    }

    float move_length = Vector3Length(movement);
    if (move_length < 0.0001f) {
        Entity temp_entity = *moving_entity;
        temp_entity.position = Vector3Add(moving_entity->position, movement);
        CollisionResult temp_result = aabb_is_moving ? 
            Collision_checkMixed(&temp_entity, cylinder, true) :
            Collision_checkMixed(aabb, &temp_entity, true);

        
        if (temp_result.hit) {
            temp_result.entity = static_entity;  // Point to the static entity that was hit
            return temp_result;
        }
        return temp_result;
    }

    /* Create expanded AABB (add cylinder radius to XZ, keep Y as-is) */
	Vector3 
		cyl_pos  = cylinder->position,
		aabb_pos = aabb->position;
	float 
		cyl_radius = cylinder->radius * 0.5f,
		cyl_height = cylinder->height * 0.5f;

	BoundingBox expanded_box = {
			{
				aabb_pos.x - (aabb->bounds.x * 0.5f) - cyl_radius,
				aabb_pos.y - (aabb->bounds.y * 0.5f) - cyl_height,
				aabb_pos.z - (aabb->bounds.z * 0.5f) - cyl_radius
			},
			{
				aabb_pos.x + (aabb->bounds.x * 0.5f) + cyl_radius,
				aabb_pos.y + (aabb->bounds.y * 0.5f) + cyl_height,
				aabb_pos.z + (aabb->bounds.z * 0.5f) + cyl_radius
			}
		};

	Ray ray = { moving_entity->position, Vector3Normalize(movement) };
	RayCollision collision = GetRayCollisionBox(ray, expanded_box);

	if (collision.hit && 
		collision.distance >= 0.0f && 
        collision.distance <= move_length
    ) {
        result.hit      = true;
        result.distance = collision.distance;
        result.entity   = static_entity;
        result.position = collision.point;
        result.normal   = collision.normal;
    }

	return result;
    
	if (collision.hit && collision.distance <= move_length) {
		/* Find closest corner based on hit point */
		Vector2 
			hit_xz                  = { collision.point.x, collision.point.z },
			aabb_center             = { aabb_pos.x, aabb_pos.z },
			closest_corner_expanded = {
					(hit_xz.x < aabb_center.x) 
						? expanded_box.min.x : expanded_box.max.x,
					(hit_xz.y < aabb_center.y) 
						? expanded_box.min.z : expanded_box.max.z
				},
			closest_corner_original = {
					(hit_xz.x < aabb_center.x) 
						? aabb_pos.x - aabb->bounds.x * 0.5f 
						: aabb_pos.x + aabb->bounds.x * 0.5f,
					(hit_xz.y < aabb_center.y) 
						? aabb_pos.z - aabb->bounds.z * 0.5f 
						: aabb_pos.z + aabb->bounds.z * 0.5f
				},
			intersection;
		
		/* Check if close enough to corner for circle collision */
		if (Vector2Distance(hit_xz, closest_corner_expanded) <= cyl_radius) {
			Vector2 
				movement_2d = { movement.x, movement.z },
				cyl_center  = { cyl_pos.x, cyl_pos.z };
			
			CollisionResult circle_result;
			circle_result.hit = CheckCollisionRay2dCircle(
					(Ray2d) {
							cyl_center,
							movement_2d
						},
					closest_corner_original,
					cyl_radius,
					&intersection
				);
			
			circle_result.distance = Vector2Length(Vector2Subtract(intersection, cyl_center));
			
			if (circle_result.hit && circle_result.distance <= move_length) {
				result = circle_result;
				result.entity = static_entity;
				/* Convert 2D position back to 3D */
				float t = circle_result.distance / move_length;
				result.position = Vector3Add(moving_entity->position, Vector3Scale(movement, t));
			}
		} 
		else {
			/* Regular edge collision - use raylib result */
			result.hit      = true;
			result.distance = collision.distance;
			result.position = collision.point;
			result.normal   = collision.normal;
			result.entity   = static_entity;
		}
	}

    return result;
}

/* CCD: Dispatch based on shape types */
CollisionResult
Collision_checkContinuous(Entity *a, Entity *b, Vector3 movement)
{
    if (
    	   !(a->collision.masks & b->collision.layers) 
    	&& !(b->collision.masks & a->collision.layers)
    ) {
        return NO_COLLISION;
    }

	uint8 colliders = COLLIDERS(a->collision_shape, b->collision_shape);

	switch (colliders) {
	case COLLIDERS(COLLISION_BOX,      COLLISION_BOX):
		return Collision_checkContinuousAABB(a, b, movement);
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_CYLINDER):
        return Collision_checkContinuousAABBCylinder(a, b, movement, true); /* AABB moving */
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_CYLINDER):
        return Collision_checkContinuousCylinder(a, b, movement);
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_BOX):
        return Collision_checkContinuousAABBCylinder(a, b, movement, false); /* Cylinder moving */
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_SPHERE): /* FALLTHROUGH */
        return Collision_checkContinuousSphere(a, b, movement);
        break;
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_BOX):
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_CYLINDER):
	default:
		break;
	}
	
	return NO_COLLISION;
}

/* Primary method for moving entities with CCD */
CollisionResult
CollisionScene__moveEntity(
    CollisionScene *scene, 
    Entity         *entity, 
    Vector3         movement
)
{
    CollisionResult result = {0};
    result.hit = false;

    float move_length = Vector3Length(movement);
    result.distance = move_length;
    
    if (move_length < 0.0001f) {
        Vector3 to = Vector3Add(entity->position, movement);
        return CollisionScene__checkCollision(scene, entity, to);
    }

    /* First check if we're currently overlapping (discrete check at current position) */
    CollisionResult overlap_check = CollisionScene__checkCollision(scene, entity, entity->position);
    
    if (overlap_check.hit && overlap_check.entity && overlap_check.entity->solid) {
        /* We're stuck inside something - this is a special case, always allow movement away */
        Vector3 to_other = Vector3Subtract(overlap_check.entity->position, entity->position);
        Vector3 movement_normalized = Vector3Normalize(movement);
        Vector3 to_other_normalized = Vector3Normalize(to_other);
        
        /* If moving away from the object we're stuck in, allow it */
        float dot = Vector3DotProduct(movement_normalized, to_other_normalized);
        if (dot < -0.1f) {
            /* Moving away from overlap - allow the movement */
            result.hit = false;
            result.distance = 1.0f;
            return result;
        }
    }

    /* Check if we're moving toward any objects */
    Vector3 e_bounds = entity->bounds;
    Vector3 b_offset = entity->bounds_offset;
    Vector3 from = entity->position;
    Vector3 to = Vector3Add(from, movement);

    /* Expand query bounds to cover entire swept path */
    BoundingBox bounds = { 
			.min = Vector3Add(
				Vector3Subtract(
					Vector3Min(from, to), 
					Vector3Scale(e_bounds, 0.5f)
				), 
				b_offset
			),
			.max = Vector3Add(
				Vector3Add(
					Vector3Max(from, to), 
					Vector3Scale(e_bounds, 0.5f)
				), 
				b_offset
			)
		};
    
    Entity **candidates = CollisionScene__queryRegion(
			scene, 
			bounds
		);
    
    for (int i = 0; i < DynamicArray_length(candidates); i++) {
        Entity *other = candidates[i];
        if (other == entity || !other->collision_shape) continue;

        /* Check direction of movement relative to this object */
        Vector3 to_other = Vector3Subtract(other->position, entity->position);
        Vector3 movement_normalized = Vector3Normalize(movement);
        Vector3 to_other_normalized = Vector3Normalize(to_other);
        
        float dot = Vector3DotProduct(movement_normalized, to_other_normalized);
        
        /* Only use CCD if we're moving toward the object */
        if (dot > 0.1f) {
            /* Moving toward object - use continuous collision detection */
            Entity temp_entity = *entity;
            CollisionResult test_result = Collision_checkContinuous(&temp_entity, other, movement);
            
            if (test_result.hit && test_result.distance < result.distance) {
                result = test_result;
            }
        } else {
            /* Moving away from or parallel to object - check if final position would overlap */
            Entity temp_entity = *entity;
            temp_entity.position = to;
            
            CollisionResult final_check = Collision_checkDiscreet(&temp_entity, other);
            
            if (final_check.hit && other->solid) {
                /* Would still be overlapping - this is a problem */
                /* But only if the object is solid */
                result = final_check;
                result.distance = 0.0f; /* Can't move at all */
                break;
            }
            /* If not solid or no overlap at final position, allow the movement */
        }
    }
	
	DynamicArray_free(candidates);
    return result;
}


/***************
	RAYCASTS
***************/
static CollisionResult
checkRayOrSphere(K_Ray ray, Entity *entity, bool AABB)
{
	CollisionResult result;
	
	result.ray_collision = (AABB) 
		? GetRayCollisionBox(ray.ray, Entity_getBoundingBox(entity))
		: GetRayCollisionSphere(ray.ray, entity->position, entity->bounds.x);
	
	if (!result.hit || ray.length < result.distance) return NO_COLLISION;
	
	result.entity = entity;
	
	return result;
}

CollisionResult
Collision_checkRayAABB(K_Ray ray, Entity *entity)
{
	return checkRayOrSphere(ray, entity, true);
}

CollisionResult
Collision_checkRaySphere(K_Ray ray, Entity *entity)
{
	return checkRayOrSphere(ray, entity, false);
}

CollisionResult
Collision_checkRayCylinder(K_Ray ray, Entity *entity)
{
    CollisionResult result = {0};
    
    Vector3 cylinder_center = entity->position;
    float
		radius = entity->bounds.x,  /* x field serves as radius */
		height = entity->bounds.y,  /* y field is height */
    
    /* Cylinder extends from center.y to center.y + height (assuming bottom-centered) */
		cylinder_bottom = cylinder_center.y,
		cylinder_top    = cylinder_center.y + height;
    
    /* Ray components */
    Vector3 
		ray_start = ray.position,
		ray_dir   = ray.direction;
    
		/* Project ray onto XZ plane (ignore Y component for cylinder side collision) */
	Vector2
		ray_start_xz = {ray_start.x, ray_start.z},
		ray_dir_xz = {ray_dir.x, ray_dir.z},
		cylinder_center_xz = {cylinder_center.x, cylinder_center.z};
    
    /* Check if ray direction has any XZ component (not purely vertical) */
    float 
		xz_length_sq = ray_dir_xz.x * ray_dir_xz.x + ray_dir_xz.y * ray_dir_xz.y,
    
		t_cylinder   = INFINITY;
		
    Vector3 
		hit_point  = {0},
		hit_normal = {0};
    
    /* Check collision with cylinder sides (infinite cylinder in XZ plane) */
    if (xz_length_sq > 0.0001f) { /* Not purely vertical ray */
        /* Normalize XZ direction */
        float xz_length = sqrtf(xz_length_sq);
        Vector2 
			ray_dir_xz_norm = {ray_dir_xz.x / xz_length, ray_dir_xz.y / xz_length},
        
			/* Vector from cylinder center to ray start in XZ plane */
			to_ray = {ray_start_xz.x - cylinder_center_xz.x, 
                          ray_start_xz.y - cylinder_center_xz.y};
        
        /* Quadratic equation coefficients for ray-circle intersection */
        /* (ray_start + t * ray_dir - center)^2 = radius^2 */
        float 
			a = ray_dir_xz_norm.x * ray_dir_xz_norm.x + ray_dir_xz_norm.y * ray_dir_xz_norm.y,
			b = 2.0f * (to_ray.x * ray_dir_xz_norm.x + to_ray.y * ray_dir_xz_norm.y),
			c = to_ray.x * to_ray.x + to_ray.y * to_ray.y - radius * radius,
        
			discriminant = b * b - 4 * a * c;
        
        if (discriminant >= 0) {
            float 
				sqrt_disc = sqrtf(discriminant),
				t1        = (-b - sqrt_disc) / (2 * a),
				t2        = (-b + sqrt_disc) / (2 * a),
            
				/* We want the closest positive intersection */
				t_side    = (t1 > 0) ? t1 : t2;
            
            if (t_side > 0) {
                /* Convert back to 3D space */
                t_cylinder = t_side / xz_length; /* Scale by actual 3D ray length */
                
                hit_point = Vector3Add(ray_start, Vector3Scale(ray_dir, t_cylinder));
                
                /* Check if hit point is within cylinder height bounds */
                if (hit_point.y >= cylinder_bottom && hit_point.y <= cylinder_top) {
                    /* Calculate normal (pointing outward from cylinder axis) */
                    Vector2 
						hit_xz    = {hit_point.x, hit_point.z},
						normal_xz = {hit_xz.x - cylinder_center_xz.x, 
                                        hit_xz.y - cylinder_center_xz.y};
                    float normal_length = sqrtf(normal_xz.x * normal_xz.x + normal_xz.y * normal_xz.y);
                    if (normal_length > 0) {
                        hit_normal = (Vector3){normal_xz.x / normal_length, 0, normal_xz.y / normal_length};
                    }
                } else {
                    t_cylinder = INFINITY; /* Hit outside height bounds */
                }
            }
        }
    }
    
    /* Check collision with top and bottom caps */
    float t_caps = INFINITY;
    
    if (fabsf(ray_dir.y) > 0.0001f) { /* Ray has Y component */
        /* Check bottom cap (y = cylinder_bottom) */
        float t_bottom = (cylinder_bottom - ray_start.y) / ray_dir.y;
        if (t_bottom > 0) {
            Vector3 bottom_hit = Vector3Add(
					ray_start, 
					Vector3Scale(
							ray_dir, 
							t_bottom
						)
					);
            float dist_from_axis_sq = (bottom_hit.x - cylinder_center.x) 
									* (bottom_hit.x - cylinder_center.x) 
									+ (bottom_hit.z - cylinder_center.z) 
									* (bottom_hit.z - cylinder_center.z);
            if (dist_from_axis_sq <= radius * radius) {
                if (t_bottom < t_caps) {
                    t_caps = t_bottom;
                    hit_point = bottom_hit;
                    hit_normal = (Vector3){0, -1, 0}; /* Bottom cap normal points down */
                }
            }
        }
        
        /* Check top cap (y = cylinder_top) */
        float t_top = (cylinder_top - ray_start.y) / ray_dir.y;
        if (t_top > 0) {
            Vector3 top_hit = Vector3Add(
					ray_start, 
					Vector3Scale(
							ray_dir, 
							t_top
						)
					);
            float dist_from_axis_sq = (top_hit.x - cylinder_center.x) 
									* (top_hit.x - cylinder_center.x) 
									+ (top_hit.z - cylinder_center.z) 
									* (top_hit.z - cylinder_center.z);
            if (dist_from_axis_sq <= radius * radius) {
                if (t_top < t_caps) {
                    t_caps = t_top;
                    hit_point = top_hit;
                    hit_normal = (Vector3){0, 1, 0}; /* Top cap normal points up */
                }
            }
        }
    }
    
    /* Choose the closest valid intersection */
    float final_t = fminf(t_cylinder, t_caps);
    
    if (final_t == INFINITY || final_t > ray.length || final_t <= 0) {
        return (CollisionResult){0}; /* NO_COLLISION equivalent */
    }
    
    /* Fill result */
    result.hit = true;
    result.distance = final_t;
    result.position = Vector3Add(
			ray_start, 
			Vector3Scale(
					ray_dir, 
					final_t
				)
			);
    
    /* Recalculate normal for the chosen intersection */
    if (final_t == t_caps) {
        /* Normal already set above for caps */
        result.normal = hit_normal;
    } else {
        /* Side collision normal */
        Vector2 
			hit_xz    = {result.position.x, result.position.z},
			center_xz = {cylinder_center.x, cylinder_center.z},
			normal_xz = {hit_xz.x - center_xz.x, hit_xz.y - center_xz.y};
        float normal_length = sqrtf(normal_xz.x * normal_xz.x + normal_xz.y * normal_xz.y);
        if (normal_length > 0) {
            result.normal = (Vector3){normal_xz.x / normal_length, 0, normal_xz.y / normal_length};
        }
    }
    
    result.entity = entity;
    result.material_id = 0;
    result.user_data = NULL;
    
    return result;
}


/* Simple raycast */
CollisionResult
CollisionScene__raycast(CollisionScene *scene, K_Ray ray)
{
	CollisionResult closest_result = {0};
	closest_result.hit      = false;
	closest_result.distance = INFINITY;

	Vector3 to = Vector3Add(
			ray.position, 
			Vector3Scale(
				Vector3Normalize(ray.direction), 
				ray.length
			)
		);
	/* Get bounds along ray path */
	BoundingBox bbox = {
			.min = Vector3Min(ray.position, to),
			.max = Vector3Max(ray.position, to)
		};
	
	/* Query spatial hash */
	Entity **candidates = CollisionScene__queryRegion(
			scene, 
			bbox
		);
	
	for (int i = 0; i < DynamicArray_length(candidates); i++) {
		Entity *entity = candidates[i];

		CollisionResult result;
		switch (entity->collision_shape) {
		case COLLISION_NONE:
			continue;
			break;
		case COLLISION_BOX:
			result = Collision_checkRayAABB(    ray, entity);
			break;
		case COLLISION_CYLINDER:
			result = Collision_checkRayCylinder(ray, entity);
			break;
		case COLLISION_SPHERE:
			result = Collision_checkRaySphere(  ray, entity);
			break;
		}
		
		if (result.hit && result.distance < closest_result.distance) {
			closest_result = result;
		}
	}
	
	DynamicArray_free(candidates);
	return closest_result;
}

/* System update - called each frame */
void
CollisionScene__update(CollisionScene *self)
{
	SpatialHash_clear(self->spatial_hash);

	EntityNode *first_entity = Scene__getEntities(self->scene);
	if (!first_entity) return;

	EntityNode *current = first_entity;
	if (current) {
		do {
			Entity *entity = &current->base;

			if (entity->active && entity->collision_shape) {
				CollisionScene__insertEntity(self, entity);
			}

			current = current->next;
		} while (current != first_entity);
	}

	self->needs_rebuild = false;
}
