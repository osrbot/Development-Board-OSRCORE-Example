/*
 * Madgwick AHRS — C implementation
 * Ported from the original C++ by SOH Madgwick (GNU GPL)
 */

#include "madgwick.h"
#include <math.h>
#include "esp_timer.h"

#define DEG2RAD 0.0174533f

static float inv_sqrt(float x)
{
    float halfx = 0.5f * x;
    float y = x;
    long i = *(long *)&y;
    i = 0x5f3759df - (i >> 1);
    y = *(float *)&i;
    y = y * (1.5f - (halfx * y * y));
    y = y * (1.5f - (halfx * y * y));
    return y;
}

void madgwick_init(madgwick_t *m, float beta)
{
    m->q0 = 1.0f; m->q1 = 0.0f; m->q2 = 0.0f; m->q3 = 0.0f;
    m->beta    = beta;
    m->last_us = esp_timer_get_time();
}

void madgwick_update_imu(madgwick_t *m,
                         float gx, float gy, float gz,
                         float ax, float ay, float az)
{
    int64_t now = esp_timer_get_time();
    float dt = (now - m->last_us) / 1000000.0f;
    m->last_us = now;

    gx *= DEG2RAD; gy *= DEG2RAD; gz *= DEG2RAD;

    float q0 = m->q0, q1 = m->q1, q2 = m->q2, q3 = m->q3;

    float qDot1 = 0.5f * (-q1*gx - q2*gy - q3*gz);
    float qDot2 = 0.5f * ( q0*gx + q2*gz - q3*gy);
    float qDot3 = 0.5f * ( q0*gy - q1*gz + q3*gx);
    float qDot4 = 0.5f * ( q0*gz + q1*gy - q2*gx);

    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        float rn = inv_sqrt(ax*ax + ay*ay + az*az);
        ax *= rn; ay *= rn; az *= rn;

        float _2q0 = 2.0f*q0, _2q1 = 2.0f*q1, _2q2 = 2.0f*q2, _2q3 = 2.0f*q3;
        float _4q0 = 4.0f*q0, _4q1 = 4.0f*q1, _4q2 = 4.0f*q2;
        float _8q1 = 8.0f*q1, _8q2 = 8.0f*q2;
        float q0q0 = q0*q0, q1q1 = q1*q1, q2q2 = q2*q2, q3q3 = q3*q3;

        float s0 = _4q0*q2q2 + _2q2*ax + _4q0*q1q1 - _2q1*ay;
        float s1 = _4q1*q3q3 - _2q3*ax + 4.0f*q0q0*q1 - _2q0*ay - _4q1 + _8q1*q1q1 + _8q1*q2q2 + _4q1*az;
        float s2 = 4.0f*q0q0*q2 + _2q0*ax + _4q2*q3q3 - _2q3*ay - _4q2 + _8q2*q1q1 + _8q2*q2q2 + _4q2*az;
        float s3 = 4.0f*q1q1*q3 - _2q1*ax + 4.0f*q2q2*q3 - _2q2*ay;

        rn = inv_sqrt(s0*s0 + s1*s1 + s2*s2 + s3*s3);
        s0 *= rn; s1 *= rn; s2 *= rn; s3 *= rn;

        qDot1 -= m->beta * s0;
        qDot2 -= m->beta * s1;
        qDot3 -= m->beta * s2;
        qDot4 -= m->beta * s3;
    }

    q0 += qDot1 * dt; q1 += qDot2 * dt;
    q2 += qDot3 * dt; q3 += qDot4 * dt;

    float rn = inv_sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    m->q0 = q0*rn; m->q1 = q1*rn; m->q2 = q2*rn; m->q3 = q3*rn;
}

void madgwick_update(madgwick_t *m,
                     float gx, float gy, float gz,
                     float ax, float ay, float az,
                     float mx, float my, float mz)
{
    if (mx == 0.0f && my == 0.0f && mz == 0.0f) {
        madgwick_update_imu(m, gx, gy, gz, ax, ay, az);
        return;
    }

    int64_t now = esp_timer_get_time();
    float dt = (now - m->last_us) / 1000000.0f;
    m->last_us = now;

    gx *= DEG2RAD; gy *= DEG2RAD; gz *= DEG2RAD;

    float q0 = m->q0, q1 = m->q1, q2 = m->q2, q3 = m->q3;

    float qDot1 = 0.5f * (-q1*gx - q2*gy - q3*gz);
    float qDot2 = 0.5f * ( q0*gx + q2*gz - q3*gy);
    float qDot3 = 0.5f * ( q0*gy - q1*gz + q3*gx);
    float qDot4 = 0.5f * ( q0*gz + q1*gy - q2*gx);

    if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
        float rn = inv_sqrt(ax*ax + ay*ay + az*az);
        ax *= rn; ay *= rn; az *= rn;

        rn = inv_sqrt(mx*mx + my*my + mz*mz);
        mx *= rn; my *= rn; mz *= rn;

        float _2q0mx = 2.0f*q0*mx, _2q0my = 2.0f*q0*my, _2q0mz = 2.0f*q0*mz;
        float _2q1mx = 2.0f*q1*mx;
        float _2q0 = 2.0f*q0, _2q1 = 2.0f*q1, _2q2 = 2.0f*q2, _2q3 = 2.0f*q3;
        float _2q0q2 = 2.0f*q0*q2, _2q2q3 = 2.0f*q2*q3;
        float q0q0=q0*q0, q0q1=q0*q1, q0q2=q0*q2, q0q3=q0*q3;
        float q1q1=q1*q1, q1q2=q1*q2, q1q3=q1*q3;
        float q2q2=q2*q2, q2q3=q2*q3, q3q3=q3*q3;

        float hx = mx*q0q0 - _2q0my*q3 + _2q0mz*q2 + mx*q1q1 + 2.0f*q1*my*q2 + 2.0f*q1*mz*q3 - mx*q2q2 - mx*q3q3;
        float hy = _2q0mx*q3 + my*q0q0 - _2q0mz*q1 + _2q1mx*q2 - my*q1q1 + my*q2q2 + 2.0f*q2*mz*q3 - my*q3q3;
        float _2bx = sqrtf(hx*hx + hy*hy);
        float _2bz = -_2q0mx*q2 + _2q0my*q1 + mz*q0q0 + _2q1mx*q3 - mz*q1q1 + 2.0f*q2*my*q3 - mz*q2q2 + mz*q3q3;
        float _4bx = 2.0f*_2bx, _4bz = 2.0f*_2bz;

        float s0 = -_2q2*(2.0f*q1q3 - _2q0q2 - ax) + _2q1*(2.0f*q0q1 + _2q2q3 - ay)
                   - _2bz*q2*(_2bx*(0.5f-q2q2-q3q3) + _2bz*(q1q3-q0q2) - mx)
                   + (-_2bx*q3+_2bz*q1)*(_2bx*(q1q2-q0q3) + _2bz*(q0q1+q2q3) - my)
                   + _2bx*q2*(_2bx*(q0q2+q1q3) + _2bz*(0.5f-q1q1-q2q2) - mz);
        float s1 =  _2q3*(2.0f*q1q3 - _2q0q2 - ax) + _2q0*(2.0f*q0q1 + _2q2q3 - ay)
                   - 4.0f*q1*(1.0f-2.0f*q1q1-2.0f*q2q2 - az)
                   + _2bz*q3*(_2bx*(0.5f-q2q2-q3q3) + _2bz*(q1q3-q0q2) - mx)
                   + (_2bx*q2+_2bz*q0)*(_2bx*(q1q2-q0q3) + _2bz*(q0q1+q2q3) - my)
                   + (_2bx*q3-_4bz*q1)*(_2bx*(q0q2+q1q3) + _2bz*(0.5f-q1q1-q2q2) - mz);
        float s2 = -_2q0*(2.0f*q1q3 - _2q0q2 - ax) + _2q3*(2.0f*q0q1 + _2q2q3 - ay)
                   - 4.0f*q2*(1.0f-2.0f*q1q1-2.0f*q2q2 - az)
                   + (-_4bx*q2-_2bz*q0)*(_2bx*(0.5f-q2q2-q3q3) + _2bz*(q1q3-q0q2) - mx)
                   + (_2bx*q1+_2bz*q3)*(_2bx*(q1q2-q0q3) + _2bz*(q0q1+q2q3) - my)
                   + (_2bx*q0-_4bz*q2)*(_2bx*(q0q2+q1q3) + _2bz*(0.5f-q1q1-q2q2) - mz);
        float s3 =  _2q1*(2.0f*q1q3 - _2q0q2 - ax) + _2q2*(2.0f*q0q1 + _2q2q3 - ay)
                   + (-_4bx*q3+_2bz*q1)*(_2bx*(0.5f-q2q2-q3q3) + _2bz*(q1q3-q0q2) - mx)
                   + (-_2bx*q0+_2bz*q2)*(_2bx*(q1q2-q0q3) + _2bz*(q0q1+q2q3) - my)
                   + _2bx*q1*(_2bx*(q0q2+q1q3) + _2bz*(0.5f-q1q1-q2q2) - mz);

        rn = inv_sqrt(s0*s0 + s1*s1 + s2*s2 + s3*s3);
        s0 *= rn; s1 *= rn; s2 *= rn; s3 *= rn;

        qDot1 -= m->beta*s0; qDot2 -= m->beta*s1;
        qDot3 -= m->beta*s2; qDot4 -= m->beta*s3;
    }

    q0 += qDot1*dt; q1 += qDot2*dt;
    q2 += qDot3*dt; q3 += qDot4*dt;

    float rn = inv_sqrt(q0*q0 + q1*q1 + q2*q2 + q3*q3);
    m->q0 = q0*rn; m->q1 = q1*rn; m->q2 = q2*rn; m->q3 = q3*rn;
}

void madgwick_get_euler(const madgwick_t *m,
                        float *roll, float *pitch, float *yaw)
{
    float q0 = m->q0, q1 = m->q1, q2 = m->q2, q3 = m->q3;
    *roll  = atan2f(2.0f*(q0*q1 + q2*q3), 1.0f - 2.0f*(q1*q1 + q2*q2)) * 57.2957795f;
    *pitch = asinf(2.0f*(q0*q2 - q3*q1)) * 57.2957795f;
    *yaw   = atan2f(2.0f*(q0*q3 + q1*q2), 1.0f - 2.0f*(q2*q2 + q3*q3)) * 57.2957795f;
}
