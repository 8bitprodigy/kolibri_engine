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
#define COLLIDERS(a, b)     (((a) << 4) + (b))


typedef struct
CollisionScene
{
	SpatialHash *spatial_hash;
	Engine      *engine;
	bool         needs_rebuild; /* Flag to rebuild hash next frame */
}
CollisionScene;


/* 
	Constructor
*/
CollisionScene *
CollisionScene__new(Engine *engine)
{
	CollisionScene *scene = malloc(sizeof(CollisionScene));
	if (!scene) {
		ERR_OUT("Failed to allocate memory for CollisionScene.");
		return NULL;
	}

	scene->spatial_hash  = SpatialHash__new();
	scene->engine        = engine;
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
	if (!entity->collision_shape) return;

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
	BoundingBox bbox,
	int *count
)
{
	return (Entity**)SpatialHash__queryRegion(scene->spatial_hash, bbox, count);;
}

/* Cylinder Collision */
CollisionResult
Collision__checkCylinder(Entity *a, Entity *b)
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

CollisionResult
Collision__checkSphere(Entity *a, Entity *b)
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
Collision__checkMixed(Entity *aabb_entity, Entity *cyl_entity, bool cyl_is_b)
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
Collision__checkDiscreet(Entity *a, Entity *b)
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
		return Collision__checkAABB(a, b);
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_CYLINDER):
		return Collision__checkMixed(a, b, true);
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_CYLINDER):
		return Collision__checkCylinder(a, b);
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_BOX):
		return Collision__checkMixed(a, b, false);
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_SPHERE):
		return Collision__checkSphere(a, b);
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_BOX): /* FALLTHROUGH */
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_CYLINDER):
	default:
		break;
	}
	
	return NO_COLLISION;
}

/* Check collision for entity moving to new position */
CollisionResult
CollisionScene__checkCollision(CollisionScene *scene, Entity *entity, Vector3 to)
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
	int      candidate_count;
	Entity **candidates = SpatialHash__queryRegion(
		scene->spatial_hash,
		(BoundingBox){min_bounds, max_bounds},
		&candidate_count
	);

	/* Check AABB collision with each candidate */
	for (int i = 0; i < candidate_count; i++) {
		Entity *other = candidates[i];
		if (other == entity) continue; /* Skip self */
		if (!other->collision_shape) continue;

		CollisionResult test_result = Collision__checkDiscreet(&temp_entity, other);
		if (test_result.hit) {
			result = test_result;
			break; /* Return first collision found */
		}
	}
	
	return result;
}


/*************************************
	CONTINUOUS COLLISION DETECTION
*************************************/

/* CCD: Cylinder-Cylinder */
CollisionResult
Collision__checkContinuousCylinder(Entity *a, Entity *b, Vector3 movement)
{
	CollisionResult result = {0};
	result.hit             = false;
	result.distance        = 1.0f;

	Vector3
		from          = a->position,
		to            = Vector3Add(from, movement);
	float move_length = Vector3Length(movement);

	if (move_length < 0.0001f) {
		Entity temp_a = *a;
		temp_a.position = to;
		return Collision__checkCylinder(&temp_a, b);
	}

	/* 2D swept circle collision on X/Z plane */
	Vector2
		center_a = {from.x, from.z},
		center_b = {b->position.x, b->position.z},
		velocity = {movement.x, movement.z};

	float
		radius_a   = a->bounds.x * 0.5f,
		radius_b   = b->bounds.x * 0.5f,
		radius_sum = radius_a + radius_b;

	/* Relative motion: treat B as stationary, A as moving */
	Vector2 rel_pos = Vector2Subtract(center_a, center_b);

	/* Quadratic equation: |relpos + t*velocity|^2 = radius_sum^2 */
	float
		a_coeff = Vector2DotProduct(velocity, velocity),
		b_coeff = 2.0f *Vector2DotProduct(rel_pos, velocity),
		c_coeff = Vector2DotProduct(rel_pos, rel_pos) - radius_sum * radius_sum,

		discriminant = b_coeff * b_coeff - 4.0f * a_coeff *c_coeff;
	if (discriminant < 0.0f || fabsf(a_coeff) < 0.0001f) return result; /* No collision */

	float
		sqrt_disc = sqrtf(discriminant),
		t1        = (-b_coeff - sqrt_disc) / (2.0f * a_coeff),
		t2        = (-b_coeff + sqrt_disc) / (2.0f * a_coeff),

		t_collision = (0.0f <= t1 && t1 <= 1.0f) ? t1 : t2;
	if (t_collision < 0.0f || 1.0f < t_collision) return result;
	
	/* Check Y-axis overlap at collision time */
	float
		collision_y = from.y + movement.y * t_collision,
		a_bottom    = collision_y,
		a_top       = collision_y + a->bounds.y,
		b_bottom    = b->position.y,
		b_top       = b->position.y + b->bounds.y;

	if (b_top < a_bottom || a_top < b_bottom) return result; /* No Y-axis overlap */

	/* Collision found */
	result.hit      = true;
	result.distance = t_collision;
	result.entity   = b;
	result.position = Vector3Add(from, Vector3Scale(movement, t_collision));

	/* Calculate normal at contact point */
	Vector2 contact_pos_2d = {result.position.x, result.position.z};
	Vector2 normal_2d      = Vector2Normalize(Vector2Subtract(contact_pos_2d, center_b));
	result.normal          = (Vector3){normal_2d.x, 0.0f, normal_2d.y};
	
	return result;
}

/* CCD: AABB - AABB */
CollisionResult
Collision__checkContinuousAABB(Entity *a, Entity *b, Vector3 movement)
{
	CollisionResult result = {0};
    result.hit             = false;
    result.distance        = 1.0f;

	Vector3
		from = a->position,
		to   = Vector3Add(from, movement);
	float move_length = Vector3Length(movement);

	if (move_length < 0.0001f) {
		Entity temp_a   = *a;
		temp_a.position = to;
		return Collision__checkAABB(&temp_a, b);
	}

	/* Expand B's AABB by A's size */
	Vector3
		*b_pos    = &b->position,
		*b_bounds = &b->bounds,
		*a_bounds = &a->bounds,
		
		b_min     = {
			b_pos->x - b_bounds->x * 0.5f - a_bounds->x * 0.5f,
			b_pos->y - b_bounds->y * 0.5f - a_bounds->y * 0.5f,
			b_pos->z - b_bounds->z * 0.5f - a_bounds->z * 0.5f
		},
		b_max     = {
			b_pos->x + b_bounds->x * 0.5f + a_bounds->x * 0.5f,
			b_pos->y + b_bounds->y * 0.5f + a_bounds->y * 0.5f,
			b_pos->z + b_bounds->z * 0.5f + a_bounds->z * 0.5f
		},

	/* Ray vs expanded AABB */
		ray_dir = Vector3Normalize(movement);

	float 
		t_enter = 0.0f,
		t_exit  = move_length;

	for (int axis = 0; axis < 3; axis++) {
		float
			ray_pos = (axis == 0) ? from.x     : (axis == 1) ? from.y     : from.z,
			ray_vel = (axis == 0) ? movement.x : (axis == 1) ? movement.y : movement.z,
			box_min = (axis == 0) ? b_min.x    : (axis == 1) ? b_min.y    : b_min.z,
			box_max = (axis == 0) ? b_max.x    : (axis == 1) ? b_max.y    : b_max.z;

		if (fabsf(ray_vel) < 0.0001f) {
			if (ray_pos < box_min || box_max < ray_pos) return result;
		}
		else {
			float
				t1 = (box_min - ray_pos) / ray_vel,
				t2 = (box_max - ray_pos) / ray_vel;

			if (t2 < t1) { 
				float
					temp = t1;
				t1 = t2;
				t2 = temp;
			}

			t_enter = fmaxf(t_enter, t1);
			t_exit  = fminf(t_exit, t2);

			if (t_exit < t_enter) return result;
		}
	}

	if (0.0f <= t_enter && t_enter <= move_length) {
		result.hit      = true;
		result.distance = t_enter / move_length; /* Normalize to 0-1 */
		result.entity   = b;
		result.position = Vector3Add(from, Vector3Scale(movement, result.distance));

		/* Calculate normal */
		Vector3
			center_b   = {b_pos->x, b_pos->y + b_bounds->y * 0.5f, b_pos->z},
			to_contact = Vector3Subtract(result.position, center_b);
		float 
			abs_x = fabsf(to_contact.x), 
			abs_y = fabsf(to_contact.y),
			abs_z = fabsf(to_contact.z);

		if (abs_y < abs_x && abs_z < abs_x) {
			result.normal = (Vector3){(0 < to_contact.x) ? 1.0f : -1.0f, 0.0f, 0.0f};
		}
		else if (abs_z < abs_y) {
			result.normal = (Vector3){0.0f, (0 < to_contact.y) ? 1.0f : -1.0f, 0.0f};
		}
		else {
			result.normal = (Vector3){0.0f, 0.0f, (0 < to_contact.z) ? 1.0f : -1.0f};
		}
	}
	
    return result;
}

/* CCD: AABB - Cylinder (either can be moving) */
CollisionResult
Collision__checkContinuousMixed(Entity *aabb, Entity *cylinder, Vector3 movement, bool aabb_is_moving)
{
	DBG_OUT("Entering Collision__checkContinuousMixed(). aabb_is_moving: %s", aabb_is_moving?"true":"false");
    CollisionResult result = {0};
    result.hit             = false;
    result.distance        = 1.0f;

    float move_length = Vector3Length(movement);
    if (move_length < 0.0001f) {
    	DBG_OUT("\tUsing discreet collision");
        /* No movement, use discrete collision */
        if (aabb_is_moving) {
            Entity temp_aabb = *aabb;
            temp_aabb.position = Vector3Add(aabb->position, movement);
            return Collision__checkMixed(&temp_aabb, cylinder, true);
        } 
        else {
            Entity temp_cylinder = *cylinder;
            temp_cylinder.position = Vector3Add(cylinder->position, movement);
            return Collision__checkMixed(aabb, &temp_cylinder, true);
        }
    }

    Vector3 
        aabb_pos = aabb->position,
        cyl_pos = cylinder->position;
    
    /* Adjust positions based on which entity is moving */
    if (!aabb_is_moving) {
        /* Cylinder is moving - convert to AABB moving by inverting movement */
        movement = Vector3Scale(movement, -1.0f);
        /* Swap reference frame: cylinder becomes stationary, AABB moves relatively */
        Vector3 temp = aabb_pos;
        aabb_pos     = Vector3Add(cyl_pos, movement);
        cyl_pos      = temp;
    }

    /* Now we always have AABB moving towards stationary cylinder */
    /* Expand cylinder by AABB size for swept test */
    float expanded_radius = cylinder->bounds.x * 0.5f + fmaxf(aabb->bounds.x, aabb->bounds.z) * 0.5f;

    /* 2D swept point vs expanded circle */
    Vector2 
        aabb_center = {aabb_pos.x, aabb_pos.z},
        cyl_center  = {cyl_pos.x, cyl_pos.z},
        velocity    = {movement.x, movement.z};

    Vector2 rel_pos = Vector2Subtract(aabb_center, cyl_center);
    
    /* Quadratic collision detection */
    float
        a_coeff = Vector2DotProduct(velocity, velocity),
        b_coeff = 2.0f * Vector2DotProduct(rel_pos, velocity),
        c_coeff = Vector2DotProduct(rel_pos, rel_pos) - expanded_radius * expanded_radius,

		t_collision = 1.0f;
    bool found_collision = false;

    if (fabsf(a_coeff) < 0.0001f) {
        /* Linear motion case */
        if (fabsf(b_coeff) >= 0.0001f) {
            float t = -c_coeff / b_coeff;
            if (t >= 0.0f && t <= 1.0f) {
                t_collision = t;
                found_collision = true;
            }
        }
    } 
    else {
        float discriminant = b_coeff * b_coeff - 4.0f * a_coeff * c_coeff;
        if (discriminant >= 0.0f) {
            float 
                sqrt_disc = sqrtf(discriminant),
                t1 = (-b_coeff - sqrt_disc) / (2.0f * a_coeff),
                t2 = (-b_coeff + sqrt_disc) / (2.0f * a_coeff),

				t = (t1 >= 0.0f && t1 <= 1.0f) ? t1 : t2;
            if (t >= 0.0f && t <= 1.0f) {
                t_collision = t;
                found_collision = true;
            }
        }
    }

    if (!found_collision) return result;

    /* Check Y overlap at collision time */
    float 
        collision_y = aabb_pos.y + movement.y * t_collision,
        aabb_bottom = collision_y,
        aabb_top    = collision_y + aabb->bounds.y,
        cyl_bottom  = cyl_pos.y,
        cyl_top     = cyl_pos.y + cylinder->bounds.y;

    if (aabb_bottom > cyl_top || aabb_top < cyl_bottom) return result;

    /* Collision found */
    result.hit      = true;
    result.distance = t_collision;
    result.entity   = aabb_is_moving ? cylinder : aabb; /* Return the non-moving entity */
    
    /* Calculate contact position in world space */
    if (aabb_is_moving) {
        result.position = Vector3Add(aabb->position, Vector3Scale(movement, t_collision));
    } else {
        result.position = Vector3Add(cylinder->position, Vector3Scale(movement, t_collision));
    }

    /* Calculate normal - always points from cylinder center toward contact */
    Vector2 
        contact_2d = {result.position.x, result.position.z},
        normal_2d  = Vector2Normalize(Vector2Subtract(contact_2d, cyl_center));
    
    /* Adjust normal direction based on which entity is moving */
    float normal_sign = aabb_is_moving ? 1.0f : -1.0f;
    result.normal     = (Vector3){normal_2d.x * normal_sign, 0.0f, normal_2d.y * normal_sign};
	
	DBG_OUT("\tresult.hit: %s", result.hit?"true":"false");
    return result;
}

/* CCD: Dispatch based on shape types */
CollisionResult
Collision__checkContinuous(Entity *a, Entity *b, Vector3 movement)
{
	DBG_OUT("Collision__checkContinuous() entered...");
    if (
    	   !(a->collision.masks & b->collision.layers) 
    	&& !(b->collision.masks & a->collision.layers)
    ) {
    	DBG_OUT("NO COLLISION. Exiting early.");
        return NO_COLLISION;
    }

	uint8 colliders = COLLIDERS(a->collision_shape, b->collision_shape);

	switch (colliders) {
	case COLLIDERS(COLLISION_BOX,      COLLISION_BOX):
		DBG_OUT("Box-Box");
		return Collision__checkContinuousAABB(a, b, movement);
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_CYLINDER):
		DBG_OUT("Box-Cylinder");
        return Collision__checkContinuousMixed(a, b, movement, true); /* AABB moving */
		break;
	case COLLIDERS(COLLISION_BOX,      COLLISION_SPHERE):
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_CYLINDER):
		DBG_OUT("Cylinder-Cylinder");
        return Collision__checkContinuousCylinder(a, b, movement);
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_BOX):
		DBG_OUT("Cylinder-Box");
		DBG_OUT("Well, it should show up here...");
        return Collision__checkContinuousMixed(b, a, movement, false); /* Cylinder moving */
		break;
	case COLLIDERS(COLLISION_CYLINDER, COLLISION_SPHERE):
		DBG_OUT("Cylinder-Sphere");
		break;
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_SPHERE): /* FALLTHROUGH */
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_BOX):
	case COLLIDERS(COLLISION_SPHERE,   COLLISION_CYLINDER):
	default:
		break;
	}

	DBG_OUT("NO COLLISION. At least we tried, this time...");
	return NO_COLLISION;
}

/* Primary method for moving entities with CCD */
CollisionResult
CollisionScene__moveEntity(CollisionScene *scene, Entity *entity, Vector3 movement)
{
	DBG_OUT("Entered CollisionScene__moveEntity()...");
    CollisionResult result = {0};
    result.hit = false;
    result.distance = 1.0f;

    float move_length = Vector3Length(movement);
    
    if (move_length < 0.0001f) {
        Vector3 to = Vector3Add(entity->position, movement);
        return CollisionScene__checkCollision(scene, entity, to);
    }

    Vector3 
		e_bounds = entity->bounds,
		b_offset = entity->bounds_offset,
		from     = entity->position,
		to       = Vector3Add(from, movement);


    /* Expand query bounds to cover entire swept path */
    BoundingBox bounds = { 
			.min = {
				fminf(from.x, to.x) - (e_bounds.x * 0.5f) + b_offset.x,
				fminf(from.y, to.y) - (e_bounds.y * 0.5f) + b_offset.y,
				fminf(from.z, to.z) - (e_bounds.z * 0.5f) + b_offset.z
			},
			.max = {
				fmaxf(from.x, to.x) + (e_bounds.x * 0.5f) + b_offset.x,
				fmaxf(from.y, to.y) + (e_bounds.y * 0.5f) + b_offset.y,
				fmaxf(from.z, to.z) + (e_bounds.z * 0.5f) + b_offset.z
			}
        };

	DBG_OUT(
			"Bounding box:\n\tmin - x: %.8f\ty: %.8f\tz: %.8f\n\tmax - x: %.8f\ty: %.8f\tz: %.8f",
			bounds.min.x, bounds.min.y, bounds.min.z,    bounds.max.x, bounds.max.y, bounds.max.z
		);
	
    int candidate_count;
    Entity **candidates = SpatialHash__queryRegion(
			scene->spatial_hash, 
			bounds, 
			&candidate_count
		);
	
	DBG_OUT("Num candidates: %d", candidate_count);
    for (int i = 0; i < candidate_count; i++) {
        Entity *other = candidates[i];
        if (other == entity || !other->collision_shape) continue;

        /* Create temporary entity for CCD */
        Entity temp_entity = *entity;

        CollisionResult test_result = Collision__checkContinuous(&temp_entity, other, movement);
        
        if (test_result.hit && test_result.distance < result.distance) result = test_result;
    }

	DBG_OUT("Result: %d", result.hit);
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
Collision__checkRayAABB(K_Ray ray, Entity *entity)
{
	return checkRayOrSphere(ray, entity, true);
}

CollisionResult
Collision__checkRaySphere(K_Ray ray, Entity *entity)
{
	return checkRayOrSphere(ray, entity, false);
}

CollisionResult
Collision__checkRayCylinder(K_Ray ray, Entity *entity)
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
		ray_dir   = ray.direction,
    
		/* Project ray onto XZ plane (ignore Y component for cylinder side collision) */
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
            Vector3 bottom_hit = Vector3Add(ray_start, Vector3Scale(ray_dir, t_bottom));
            float dist_from_axis_sq = (bottom_hit.x - cylinder_center.x) * (bottom_hit.x - cylinder_center.x) +
                                     (bottom_hit.z - cylinder_center.z) * (bottom_hit.z - cylinder_center.z);
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
            Vector3 top_hit = Vector3Add(ray_start, Vector3Scale(ray_dir, t_top));
            float dist_from_axis_sq = (top_hit.x - cylinder_center.x) * (top_hit.x - cylinder_center.x) +
                                     (top_hit.z - cylinder_center.z) * (top_hit.z - cylinder_center.z);
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
    result.position = Vector3Add(ray_start, Vector3Scale(ray_dir, final_t));
    
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
Collision__raycast(CollisionScene *scene, K_Ray ray)
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
			{
				fminf(ray.position.x, to.x),
				fminf(ray.position.y, to.y),
				fminf(ray.position.z, to.z)
			},
			{
				fmaxf(ray.position.x, to.x),
				fmaxf(ray.position.y, to.y),
				fmaxf(ray.position.z, to.z)
			}
		};
	
	/* Query spatial hash */
	int      candidate_count;
	Entity **candidates = CollisionScene__queryRegion(
		scene, 
		bbox,
		&candidate_count
	);
	
	for (int i = 0; i < candidate_count; i++) {
		Entity *entity = candidates[i];

		CollisionResult result;
		switch (entity->collision_shape) {
		case COLLISION_NONE:
			continue;
			break;
		case COLLISION_BOX:
			result = Collision__checkRayAABB(    ray, entity);
			break;
		case COLLISION_CYLINDER:
			result = Collision__checkRayCylinder(ray, entity);
			break;
		case COLLISION_SPHERE:
			result = Collision__checkRaySphere(  ray, entity);
			break;
		}
		
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

	EntityNode *first_entity = Engine__getEntities(scene->engine);
	if (!first_entity) return;

	EntityNode *current = first_entity;
	if (current) {
		do {
			Entity *entity = &current->base;

			if (entity->active && entity->collision_shape) {
				CollisionScene__insertEntity(scene, entity);
			}

			current = current->next;
		} while (current != first_entity);
	}

	scene->needs_rebuild = false;
}
