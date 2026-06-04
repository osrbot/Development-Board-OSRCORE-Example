#pragma once
#include <stdbool.h>

void  imu_heater_init(float target_c);
void  imu_heater_update_temp(float t); // feed current temp from IMU task
bool  imu_heater_is_on(void);
bool  imu_heater_warm(void);           // >= 38°C
bool  imu_heater_stable(void);         // >= 54°C
bool  imu_heater_ready(void);          // slope stable at target, or timeout
