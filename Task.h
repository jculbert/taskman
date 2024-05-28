#ifndef TASK_H_
#define TASK_H_

#include <string>

#ifdef __cplusplus
extern "C"
{
#endif

#include "zigbee_app_framework_event.h"

#ifdef __cplusplus
}
#endif

class TaskMan;

#define SuccessOrExit(aStatus) \
    do                         \
    {                          \
        if ((aStatus) != OT_ERROR_NONE)    \
        {                      \
            goto exit;         \
        }                      \
    } while (false)

class Task
{
public:
  uint8_t endpoint;
  std::string name;
  enum LOG_LEVEL {LOG_CRIT, LOG_INFO, LOG_DEBUG} log_level;
  TaskMan *taskman;
  bool needs_process;
  sl_zigbee_event_t event;

  Task (uint8_t endpoint, const char * name, enum LOG_LEVEL log_level);

  void request_process(unsigned delayms);

  virtual void process() = 0;

  void log_crit(const char *format, ...);
  void log_info(const char *format, ...);
  void log_debug(const char *format, ...);
};

#endif /* TASK_H_ */
