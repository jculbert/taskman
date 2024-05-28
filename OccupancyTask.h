#ifndef OCCUPANCY_H_
#define OCCUPANCY_H_

#include "Task.h"

class OccupancyTask : public Task
{
public:
  uint8_t occupancy; // bit map using bit 0
  enum {MOTION_NONE, MOTION_BLANKING, MOTION_DELAY} motion_state;
  uint32_t timeout; // period of no motion before sending state off
  uint32_t blanking_time; // period for disabling interrupts after motion detected

  OccupancyTask (uint8_t endpoint, enum LOG_LEVEL log_level, uint32_t timeout);

  void process();

  void update_occupancy (bool occupancy);
};

#endif /* OCCUPANCY_H_ */
