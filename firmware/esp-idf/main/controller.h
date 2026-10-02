#pragma once
#include "vehicle_config.h"
typedef enum { STOPPED, ARMED, RUNNING } VehicleState;
void vehicle_init(void);
void vehicle_poll(void);
// source=0 is USB Serial; nonzero source is a phone session. Only the owner
// that ARMed can RUN/control/refresh heartbeat. STOP works from any source.
bool vehicle_execute(const char* line, uint32_t source, uint32_t sequence);
void vehicle_stop(const char* reason);
typedef struct {
    VehicleState state;
    uint32_t battery_mv, owner, stop_count;
    int steer_deg, gas_percent, esc_percent;
    const char* last_stop;
    char reply[128];
} vehicle_status_t;
vehicle_status_t vehicle_status(void);
