#include "pid.h"
#include <math.h>
#include <string.h>

void pid_init(pid_ctrl_t *p, float kp, float ki, float kd,
              float max_integral, float deadband)
{
    memset(p, 0, sizeof(*p));
    p->kp           = kp;
    p->ki           = ki;
    p->kd           = kd;
    p->max_integral = max_integral;
    p->deadband     = deadband;
}

int pid_update(pid_ctrl_t *p, float target, float current, int neutral, float dt_sec)
{
    if (dt_sec <= 0.0f)
        return neutral;

    float err = target - current;

    if (fabsf(err) < p->deadband)
        err = 0.0f;

    p->integral += err * dt_sec;
    if (p->integral >  p->max_integral) p->integral =  p->max_integral;
    if (p->integral < -p->max_integral) p->integral = -p->max_integral;

    float derivative = (err - p->last_error) / dt_sec;
    p->last_error = err;

    float output = p->kp * err + p->ki * p->integral + p->kd * derivative;
    int pulse = neutral + (int)output;
    if (pulse < 1000) pulse = 1000;
    if (pulse > 2000) pulse = 2000;
    return pulse;
}

void pid_reset(pid_ctrl_t *p)
{
    p->integral   = 0.0f;
    p->last_error = 0.0f;
}
