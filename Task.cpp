#include <stdio.h>
#include <string.h>

#include "SEGGER_RTT.h"

#include <Task.h>
#include <TaskMan.h>

#define LOG_BUFFER_SIZE (128)
//#define RTT_COLOR_CODE_CYAN "\x1B[1;36m"

void log_rtt(const std::string &level, std::string &name, const char *format, va_list ap)
{
    unsigned length = 0;
    char buf[LOG_BUFFER_SIZE + 1];

    long unsigned int now = 0;
      //length = snprintf(buf, LOG_BUFFER_SIZE, "%s[%010lu] %s %s: ", RTT_COLOR_CODE_CYAN, now, level.c_str(), name.c_str());
    length = snprintf(buf, LOG_BUFFER_SIZE, "[%010lu] %s %s: ", now, level.c_str(), name.c_str());

    length += vsnprintf(&buf[length], (size_t)(LOG_BUFFER_SIZE - length), format, ap);

    if (length > LOG_BUFFER_SIZE)
        length = LOG_BUFFER_SIZE;
    buf[length++] = '\n';

    // Write user log to the RTT memory block.
    SEGGER_RTT_WriteNoLock(0, buf, length);
}

Task::Task (uint8_t _endpoint, const char *_name, enum LOG_LEVEL _log_level)
  : endpoint(_endpoint), name(_name), log_level(_log_level), taskman(TaskMan::GetInstance()), needs_process(false)
{
    taskman->add_task(this);
}

void
Task::request_process (unsigned delayms)
{
    taskman->request_process(this, delayms);
}

void Task::log_crit(const char *format, ...)
{
    static std::string level("[CRIT]");

    va_list args;
    va_start(args, format);
    log_rtt(level, name, format, args);
    va_end(args);
}

void Task::log_info(const char *format, ...)
{
    if (log_level < LOG_INFO)
        return;

    static std::string level("[INFO]");

    va_list args;
    va_start(args, format);
    log_rtt(level, name, format, args);
    va_end(args);
}

void Task::log_debug(const char *format, ...)
{
    if (log_level < LOG_DEBUG)
        return;

    static std::string level("[DEBG]");

    va_list args;
    va_start(args, format);
    log_rtt(level, name, format, args);
    va_end(args);
}
