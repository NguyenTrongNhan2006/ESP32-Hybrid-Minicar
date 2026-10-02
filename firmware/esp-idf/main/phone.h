#pragma once
#include "controller.h"
bool phone_init(void);
void phone_poll(void); // Called only by the same task that runs vehicle_poll.
