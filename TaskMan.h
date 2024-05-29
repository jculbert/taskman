#ifndef TASKMAN_H_
#define TASKMAN_H_

#include <list>

class Task;

class TaskMan
{
public:
  std::list <Task*> tasks;
  bool recursive_process; // true to cause running of process loop again

  TaskMan ();

  virtual ~TaskMan ();

  static TaskMan* GetInstance();

  void add_task(Task *task);

  void request_process(Task *task, uint32_t delayms);

  void process();

  void event_handler(sl_zigbee_event_t *event);
};

#endif /* TASKMAN_H_ */
