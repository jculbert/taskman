#ifndef CLUSTER_LIB_TEMPERATURETASK_H_
#define CLUSTER_LIB_TEMPERATURETASK_H_

#include "Task.h"

class TemperatureTask : public Task
{
public:
    int16_t temperature;
    uint32_t period_ms;

    TemperatureTask(uint8_t _endpoint, enum LOG_LEVEL log_level, uint32_t period_mins);

    void process();

    void update_temperature(int16_t t);
};

#endif /* CLUSTER_LIB_TEMPERATURETASK_H_ */
