#pragma once

typedef struct {
    float kp, ki, kd;
    float max_integral;
    float deadband;
    float integral;
    float last_error;
} pid_t;

void pid_init(pid_t *p, float kp, float ki, float kd,
              float max_integral, float deadband);
float pid_calc(pid_t *p, float target, float measured, float dt);
void pid_reset(pid_t *p);
