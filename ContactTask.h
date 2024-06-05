#ifndef CONTACTTASK_H_
#define CONTACTTASK_H_

#include "Task.h"

class ContactTask : public Task
{
public:
    bool state;

    ContactTask(uint8_t endpoint, enum LOG_LEVEL log_level);

    void process();
};

#endif /* CONTACTTASK_H_ */
