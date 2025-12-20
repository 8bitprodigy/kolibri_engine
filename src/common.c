#include "common.h"


int 
nextPrime(int n) 
{
    if (n <= 1) return 2;
    if (n <= 3) return n;
    if (n % 2 == 0) n++;
    
    while (true) {
        bool is_prime = true;
        for (int i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                is_prime = false;
                break;
            }
        }
        if (is_prime) return n;
        n += 2;
    }
}

inline float
invLerp(float a, float b, float value)
{
	if (a==b) return 0.0f;
	return (value - a) / (b - a);
}

inline void
moveCamera(Camera *cam, Vector3 new_position)
{
	Vector3 diff  = Vector3Normalize(Vector3Subtract(cam->target, cam->position));
	cam->position = new_position;
	cam->target   = Vector3Add(new_position, diff);
}

inline float
sig(float x) 
{
    return 1.0f / (1.0f + expf(-x));
}

inline float
sig_fast(float x) 
{
	return 0.5f * (x / (1.0f + fabsf(x))) + 0.5f;;
}


