#ifndef BATTERYTASK_H_
#define BATTERYTASK_H_

#include "Task.h"

class BatteryTask : public Task
{
private:
    uint32_t nominal_mv;
    uint32_t refresh_ms;
    bool waiting_adc;
public:
    uint8_t battery_percent;

public:
    BatteryTask(uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t nomimal_mv, uint32_t refresh_minutes);

    void process();
};

#endif /* BATTERYTASK_H_ */
