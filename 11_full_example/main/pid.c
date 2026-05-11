#include "pid.h"
#include <math.h>
#include <string.h>

void pid_init(pid_t *p, float kp, float ki, float kd,
              float max_integral, float deadband)
{
    memset(p, 0, sizeof(*p));
    p->kp           = kp;
    p->ki           = ki;
    p->kd           = kd;
    p->max_integral = max_integral;
    p->deadband     = deadband;
}

float pid_calc(pid_t *p, float target, float measured, float dt)
{
    float err = target - measured;

    if (fabsf(err) < p->deadband)
        err = 0.0f;

    p->integral += err * dt;
    if (p->integral >  p->max_integral) p->integral =  p->max_integral;
    if (p->integral < -p->max_integral) p->integral = -p->max_integral;

    float derivative = (err - p->last_error) / dt;
    p->last_error = err;

    return p->kp * err + p->ki * p->integral + p->kd * derivative;
}

void pid_reset(pid_t *p)
{
    p->integral   = 0.0f;
    p->last_error = 0.0f;
}
