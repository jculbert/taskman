#ifndef GARAGETASK_H_
#define GARAGETASK_H_

#include "Task.h"

class GarageTask : public Task
{
public:
    enum {COMMAND_NONE, COMMAND_OPEN, COMMAND_CLOSE, COMMAND_STOP} command;
    bool door_closed;
    bool target_changed;
    unsigned pulse_relay_duration;

    GarageTask(uint8_t endpoint, enum LOG_LEVEL log_level);

    void process();
};

#endif /* GARAGETASH_H_ */
