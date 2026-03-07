/**
 * Placeholder for a task
 */

#include<thread>
#include<chrono>

class Task {
  int mTaskId;

public:

  Task() : mTaskId(-1) {}
  Task(int id) : mTaskId(id) {}

  inline int id() {
     return mTaskId;
  }

  inline void perform() {
     std::this_thread::sleep_for(std::chrono::milliseconds(800));
  }

};

