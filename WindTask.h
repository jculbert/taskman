
#ifndef TASKMAN_WINDTASK_H_
#define TASKMAN_WINDTASK_H_

#include "Task.h"

class WindTask : public Task
{
private:
    uint16_t pulse_cnt_start;
    uint16_t pulse_cnt;
    uint16_t pulse_cnt_max_delta;
    uint16_t num_samples;
public:
    uint16_t wind;
    uint16_t gust;

public:
    WindTask(uint8_t endpoint, enum LOG_LEVEL log_level);

    void process_wind_data();

    void process();
};

#endif /* TASKMAN_WINDTASK_H_ */
