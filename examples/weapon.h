#ifndef WEAPON_H
#define WEAPON_H

#include "kolibri.h"
#include "projectile.h"


typedef enum
{
	ACTION_MANUAL,
	ACTION_SEMIAUTO,
	ACTION_AUTOMATIC,
}
WeaponAction;

typedef struct WeaponInfo WeaponInfo;
typedef struct WeaponData WeaponData;

typedef void (*WeaponFireCallback)(  WeaponInfo *info, WeaponData *data, Entity *source, Vector3 position, Vector3 direction);

typedef struct
WeaponInfo
{
	Model               model;
	union {
		ProjectileInfo *projectile;
		float           distance;
	};
	float               refractory_period;
	WeaponFireCallback  Fire;
	WeaponAction        action_type;
}
WeaponInfo;

typedef struct
WeaponData
{
	double  
				  trigger_down,
				  trigger_up,
				  next_shot;
	Any           data;
	int           ammo;
	bool          
				 trigger_was_down,
				 just_fired;
}
WeaponData;


void Weapon_fire(WeaponInfo *info, WeaponData *data, Entity *source, Vector3 position, Vector3 direction, bool trigger_down);


#endif /* WEAPON_H */
#ifdef WEAPON_IMPLEMENTATION

void 
Weapon_fire(
	WeaponInfo *info, 
	WeaponData *data, 
	Entity     *source, 
	Vector3     position,
	Vector3     direction,
	bool        trigger_down
)
{
    bool 
		just_pressed  = trigger_down  && !data->trigger_was_down,
		just_released = !trigger_down &&  data->trigger_was_down;

    double current_time = Engine_getTime(Entity_getEngine(source));

    if (    just_pressed ) data->trigger_down = current_time;
    else if(just_released) data->trigger_up   = current_time;
    
    switch (info->action_type) {
	case ACTION_MANUAL:
		if (just_pressed) {
			if (
				/* Check if enough time passed since trigger was released */
				current_time >= data->next_shot
				&& info->Fire
			) {
				info->Fire(info, data, source, position, direction);
				data->just_fired = true;
			}
		}
		else if (just_released && data->just_fired) {
			/* Start refractory period on release */
			data->next_shot = current_time + info->refractory_period;
			data->just_fired = false;
		}
		break;
	case ACTION_SEMIAUTO:
		if (just_pressed) {
			/* Check if enough time passed since last shot */
			if (
				current_time >= data->next_shot
				&& info->Fire
			) {
				info->Fire(info, data, source, position, direction);
				/* Refractory starts on shot */
				data->next_shot = current_time + info->refractory_period;
			}
		}
		break;
	case ACTION_AUTOMATIC:
		if (
			trigger_down
			&& current_time >= data->next_shot
		) {
			if (info->Fire) {
				info->Fire(info, data, source, position, direction);
				data->next_shot = current_time + info->refractory_period;
			}
		}
		break;
    }
    data->trigger_was_down = trigger_down;
}
	
#endif /* WEAPON_IMPLEMENTATION */
