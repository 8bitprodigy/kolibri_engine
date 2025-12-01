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
void Thinker_update(Thinker *thinker, Entity           *entity,  float game_time);


#endif /* THINKER_H */

/*====================
	IMPLEMENTATION
====================*/
#ifdef THINKER_IMPLEMENTATION

void 
Thinker_init(Thinker *thinker)
{
    thinker->function = NULL;
    thinker->nextTime = 0.0f;
    thinker->interval = 0.0f;
    thinker->userdata = NULL;
}

void 
Thinker_set(
	Thinker         *thinker, 
	ThinkerFunction  function, 
	float            delay, 
	void            *userdata
)
{
    thinker->function =  function;
    thinker->userdata =  userdata;
    thinker->interval =  0.0f;
    thinker->nextTime += delay;
}

void 
Thinker_repeat(
	Thinker         *thinker, 
	ThinkerFunction  function, 
	float            interval, 
	void            *userdata
)
{
    thinker->function =  function;
    thinker->userdata =  userdata;
    thinker->interval =  interval;
    thinker->nextTime += interval;
}

void 
Thinker_update(
	Thinker *thinker,
	Entity  *entity,
	float    game_time
)
{
	if (!thinker->function) return;
	if (game_time < thinker->next_time) return;

	thinker->function(entity, thinker->user_data);

	if (thinker->interval > 0.0f)
		thinker->next_time += thinker->interval;
	else
		thinker->function = NULL;
}
	
#endif /* THINKER_IMPLEMENTATION */
