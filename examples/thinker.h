#ifndef THINKER_H
#define THINKER_H

#include "kolibri.h"


#ifndef THINKER_SIZE
	#define THINKER_SIZE (sizeof(Thinker))
#endif
#ifndef thinker_clear
	#define thinker_clear(thinker) (thinker->function = NULL)
#endif


typedef void (*ThinkerFunction)(Entity *entity, void *user_data);

typedef struct
{
	ThinkerFunction function;
	float           next_time;  /* time to elapse before trigger */
	float           interval;   /* >0 for repeating thinkers. */
	void            *user_data; /* Optional user payload */
}
Thinker;


void Thinker_init(  Thinker *thinker);
void Thinker_set(   Thinker *thinker, ThinkerFunction  function, float delay,    void *userdata);
void Thinker_repeat(Thinker *thinker, ThinkerFunction  function, float interval, void *userdata);
void Thinker_update(Thinker *thinker, Entity           *entity);


#endif /* THINKER_H */

/*====================
	IMPLEMENTATION
====================*/
#ifdef THINKER_IMPLEMENTATION

void 
Thinker_init(Thinker *thinker)
{
    thinker->function = NULL;
    thinker->next_time = 0.0f;
    thinker->interval = 0.0f;
    thinker->user_data = NULL;
}

void 
Thinker_set(
	Thinker         *thinker, 
	ThinkerFunction  function, 
	float            delay, 
	void            *userdata
)
{
    thinker->function   = function;
    thinker->user_data  = userdata;
    thinker->interval   = 0.0f;
    thinker->next_time += delay;
}

void 
Thinker_repeat(
	Thinker         *thinker, 
	ThinkerFunction  function, 
	float            interval, 
	void            *userdata
)
{
    thinker->function   = function;
    thinker->user_data  = userdata;
    thinker->interval   = interval;
    thinker->next_time += interval;
}

void 
Thinker_update(
	Thinker *thinker,
	Entity  *entity
)
{
	if (!thinker->function) return;
	
	float game_time = Entity_getAge(entity);
	
	if (game_time < thinker->next_time) return;

	thinker->next_time = 0.0f;
	thinker->function(entity, thinker->user_data);

	if (0.0f < thinker->interval)
		thinker->next_time = game_time + thinker->interval;
}
	
#endif /* THINKER_IMPLEMENTATION */
