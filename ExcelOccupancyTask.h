#ifndef EXCELOCCUPANCY_H_
#define EXCELOCCUPANCY_H_

#include "Task.h"

class ExcelOccupancyTask : public Task
{
public:
    uint8_t occupancy; // bit map using bit 0
    enum {STATE_IDLE, STATE_BLANKING, STATE_DELAY} state;
    uint32_t timeout; // period of no motion before sending state off
    uint32_t blanking_time; // period for disabling interrupts after motion detected

    ExcelOccupancyTask (uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t timeout);

    void process();

    void update_occupancy (bool occupancy);
};

#endif /* EXCELOCCUPANCY_H_ */
