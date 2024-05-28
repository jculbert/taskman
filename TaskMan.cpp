#include "em_core.h"
#include "sl_sleeptimer.h"

#include <algorithm>

extern "C"
{
#include "zigbee_app_framework_event.h"
}

#include "TaskMan.h"
#include "Task.h"

static TaskMan taskman;

extern "C" void taskman_event_handler(sl_zigbee_event_t *event)
{
    taskman.event_handler(event);
}

extern "C" void taskman_process()
{
    taskman.process();
}

TaskMan* TaskMan::GetInstance()
{
    return &taskman;
}

TaskMan::TaskMan()
  : recursive_process(false)
{
}

TaskMan::~TaskMan()
{
}

void TaskMan::add_task(Task *task)
{
    //sl_zigbee_endpoint_event_init(&(task->event), taskman_event_handler, task->endpoint);
    sl_zigbee_event_init(&(task->event), taskman_event_handler);
    tasks.push_front(task);
}

void TaskMan::request_process(Task *task, unsigned delayms)
{
    if (delayms == 0)
    {
        if (sl_zigbee_event_is_scheduled(&(task->event)))
            sl_zigbee_event_set_inactive(&(task->event));

        // request process immediately
        task->needs_process = true;

        if (!CORE_InIrqContext())
        {
            // we are at task level, so need to loop through tasks again
            recursive_process = true;
        }
        return;
    }

    // Request process after delay
    sl_zigbee_event_set_delay_ms(&(task->event), delayms);
}

void TaskMan::process()
{
    do
    {
        recursive_process = false;

        std::list <Task*> :: iterator it;
        for(it = tasks.begin(); it != tasks.end(); ++it)
        {
            if ((*it)->needs_process)
            {
                (*it)->needs_process = false;
                (*it)->process();
            }
        }
    } while (recursive_process);
}

void TaskMan::event_handler(sl_zigbee_event_t *event)
{
    // find the task with matching event
    std::list <Task*> :: iterator it;
    for(it = tasks.begin(); it != tasks.end(); ++it)
    {
        Task *t = (*it);
        if (&(t->event) == event)
        {
            t->needs_process = true;
            break;
        }
    }
}

