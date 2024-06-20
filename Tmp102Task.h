#ifndef CLUSTER_LIB_TMP102TASK_H_
#define CLUSTER_LIB_TMP102TASK_H_

#include "Task.h"

class Tmp102Task : public Task
{
public:
    int16_t temperature;
    uint32_t period_ms;
    enum {STATE_TRIGGER_READING, STATE_READ} state;

    Tmp102Task(uint8_t _endpoint, enum LOG_LEVEL log_level, uint32_t period_mins);

    void process();
};

#endif /* CLUSTER_LIB_TMP102TASK_H_ */
