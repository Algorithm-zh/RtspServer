#ifndef BXC_RTSPSERVER_TIMER_H
#define BXC_RTSPSERVER_TIMER_H
#include <map>
#include <stdint.h>

class EventScheduler;
class Poller;
class TimerEvent;
class IOEvent;

class Timer {
public:
  typedef uint32_t TimerId;
  typedef int64_t Timestamp;     // ms
  typedef uint32_t TimeInterval; // ms

  ~Timer();

  static Timestamp getCurTime();      // 获取当前系统启动以来的毫秒数
  static Timestamp getCurTimestamp(); // 获取毫秒级时间戳（13位）

private:
  friend class TimerManager;
  Timer(TimerEvent *event, Timestamp timestamp, TimeInterval timeInterval,
        TimerId timerId);

private:
  bool handleEvent();

private:
  TimerEvent *mTimerEvent;
  Timestamp mTimestamp;
  TimeInterval mTimeInterval;
  TimerId mTimerId;

  bool mRepeat;
};

class TimerManager {
public:
  static TimerManager *createNew(EventScheduler *scheduler);

  TimerManager(EventScheduler *scheduler);
  ~TimerManager();

  Timer::TimerId addTimer(TimerEvent *event, Timer::Timestamp timestamp,
                          Timer::TimeInterval timeInterval);
  bool removeTimer(Timer::TimerId timerId);

private:
  static void readCallback(void *arg);
  void handleRead();
  void modifyTimeout();

private:
  Poller *mPoller;
  // 可以通过mTimers通过id快速查找定时器,可以通过mEvents快速查找即将到时的定时器
  std::map<Timer::TimerId, Timer> mTimers;
  std::multimap<Timer::Timestamp, Timer> mEvents;
  // mLastTimerId 记录了上一次分配的定时器标识符
  uint32_t mLastTimerId;
#ifndef WIN32
  int mTimerFd;
  IOEvent *mTimerIOEvent;
#endif // !WIN32
};

#endif // BXC_RTSPSERVER_TIMER_H
