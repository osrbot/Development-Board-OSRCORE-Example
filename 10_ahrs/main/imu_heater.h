#pragma once
#include <stdbool.h>

void  imu_heater_init(float temp_c);   // target temp, starts background task
bool  imu_heater_is_on(void);
bool  imu_heater_warm(void);           // true once >= WARM_C (38°C)
float imu_heater_get_temp(void);       // last read temp (from external update)
void  imu_heater_update_temp(float t); // called by IMU task to feed temp
