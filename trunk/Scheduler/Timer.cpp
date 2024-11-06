#include "Timer.h"
#ifndef WIN32
#include <sys/timerfd.h>
#endif // !WIN32
#include "../Base/Log.h"
#include "Event.h"
#include "EventScheduler.h"
#include "Poller.h"
#include <chrono>
#include <time.h>

// struct timespec {
//     time_t tv_sec; //Seconds
//     long   tv_nsec;// Nanoseconds
// };
// struct itimerspec {
//     struct timespec it_interval;  //Interval for periodic timer
//     （定时间隔周期） struct timespec it_value;     //Initial expiration
//     (第一次超时时间)
// };
//     it_interval不为0 表示是周期性定时器
//     it_value和it_interval都为0 表示停止定时器

static bool timerFdSetTime(int fd, Timer::Timestamp when,
                           Timer::TimeInterval period) {

#ifndef WIN32
  struct itimerspec newVal;

  // 设置首次触发时间
  newVal.it_value.tv_sec = when / 1000;                // ms->s
  newVal.it_value.tv_nsec = when % 1000 * 1000 * 1000; // ms->ns
  // 设置周期时间
  newVal.it_interval.tv_sec = period / 1000;
  newVal.it_interval.tv_nsec = period % 1000 * 1000 * 1000;
  // 设置定时器，,从当前开始计算的绝对时间，并用newVal存储新的超时时间和周期时间
  // 给fd设置新的到时时间和间隔时间
  int oldValue = timerfd_settime(fd, TFD_TIMER_ABSTIME, &newVal, NULL);
  if (oldValue < 0) {
    return false;
  }
  return true;
#endif // !WIN32

  return true;
}

Timer::Timer(TimerEvent *event, Timestamp timestamp, TimeInterval timeInterval,
             TimerId timerId)
    : mTimerEvent(event), mTimestamp(timestamp), mTimeInterval(timeInterval),
      mTimerId(timerId) {
  if (timeInterval > 0) {
    mRepeat = true; // 循环定时器
  } else {
    mRepeat = false; // 一次性定时器
  }
}

Timer::~Timer() {}
// 获取系统从启动到目前的毫秒数
Timer::Timestamp Timer::getCurTime() {
#ifndef WIN32
  // Linux系统
  struct timespec now; // tv_sec (s) tv_nsec (ns-纳秒)
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (now.tv_sec * 1000 + now.tv_nsec / 1000000);
#else
  long long now = std::chrono::steady_clock::now().time_since_epoch().count();
  return now / 1000000;
#endif // !WIN32
}
Timer::Timestamp Timer::getCurTimestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
bool Timer::handleEvent() {
  if (!mTimerEvent) {
    return false;
  }
  return mTimerEvent->handleEvent();
}

TimerManager *TimerManager::createNew(EventScheduler *scheduler) {

  if (!scheduler)
    return NULL;
  return new TimerManager(scheduler);
}

TimerManager::TimerManager(EventScheduler *scheduler)
    : mPoller(scheduler->poller()), mLastTimerId(0) {

#ifndef WIN32
  // 非windows系统可以使用描述符来实现,由网络模型调用
  //  创建由系统调用的定时器，单调递增，非阻塞，在执行exec系统调用时自动关闭文件描述符
  mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);

  if (mTimerFd < 0) {
    LOGE("create TimerFd error");
    return;
  } else {
    LOGI("fd=%d", mTimerFd);
  }
  // 为什么这里创建的是IOEvent，因为这个定时器使用的是网络模型的方式实现的，而往fd_set里添加删除就是IO事件
  mTimerIOEvent = IOEvent::createNew(mTimerFd, this);
  mTimerIOEvent->setReadCallback(readCallback);
  mTimerIOEvent->enableReadHandling();
  modifyTimeout();
  mPoller->addIOEvent(mTimerIOEvent);
#else
  scheduler->setTimerManagerReadCallback(readCallback, this);

#endif // !WIN32
}

TimerManager::~TimerManager() {
#ifndef WIN32
  mPoller->removeIOEvent(mTimerIOEvent);
  delete mTimerIOEvent;
#endif // !WIN32
}

Timer::TimerId TimerManager::addTimer(TimerEvent *event,
                                      Timer::Timestamp timestamp,
                                      Timer::TimeInterval timeInterval) {
  ++mLastTimerId;
  Timer timer(event, timestamp, timeInterval, mLastTimerId);

  mTimers.insert(std::make_pair(mLastTimerId, timer));
  // mEvents.insert(std::make_pair(TimerIndex(timestamp, mLastTimerId), timer));
  mEvents.insert(std::make_pair(timestamp, timer));
  modifyTimeout();

  return mLastTimerId;
}

bool TimerManager::removeTimer(Timer::TimerId timerId) {
  std::map<Timer::TimerId, Timer>::iterator it = mTimers.find(timerId);
  if (it != mTimers.end()) {
    mTimers.erase(timerId);
    // TODO 还需要删除mEvents的事件
  }

  modifyTimeout();

  return true;
}

// 给时间描述符设置时间戳(即首次触发时间)和周期时间
void TimerManager::modifyTimeout() {
#ifndef WIN32
  std::multimap<Timer::Timestamp, Timer>::iterator it = mEvents.begin();
  if (it != mEvents.end()) { // 存在至少一个定时器
    Timer timer = it->second;
    timerFdSetTime(mTimerFd, timer.mTimestamp, timer.mTimeInterval);
  } else {
    timerFdSetTime(mTimerFd, 0, 0);
  }
#endif // WIN32
}

void TimerManager::readCallback(void *arg) {
  TimerManager *timerManager = (TimerManager *)arg;
  timerManager->handleRead();
}

// 定时器到时，回调read
void TimerManager::handleRead() {
  // 获取当前时间戳
  Timer::Timestamp timestamp = Timer::getCurTime();

  // 检查定时器集合和事件集合是否为空
  if (!mTimers.empty() && !mEvents.empty()) {
    // 获取最早到期的定时器事件
    std::multimap<Timer::Timestamp, Timer>::iterator it = mEvents.begin();
    Timer timer = it->second;

    // 计算当前时间与定时器到期时间的差值
    int expire = timer.mTimestamp - timestamp;

    // 如果当前时间大于等于定时器的到期时间(说明已经到时了)
    if (timestamp >= timer.mTimestamp) {
      // 处理定时器事件，返回是否停止该定时器
      // handleEvent里会执行超时回调函数
      bool timerEventIsStop = timer.handleEvent();

      // 从事件集合中移除已处理的定时器
      mEvents.erase(it);

      // 如果定时器是重复的
      if (timer.mRepeat) {
        // 如果事件处理结果为停止，则从定时器集合中移除该定时器
        if (timerEventIsStop) {
          mTimers.erase(timer.mTimerId);
        } else {
          // 更新定时器的到期时间为当前时间加上间隔时间
          timer.mTimestamp = timestamp + timer.mTimeInterval;
          // 将更新后的定时器重新插入事件集合
          mEvents.insert(std::make_pair(timer.mTimestamp, timer));
        }
      } else {
        // 如果定时器不是重复的，直接从定时器集合中移除
        mTimers.erase(timer.mTimerId);
      }
    }
  }

  // 修改超时时间
  modifyTimeout();
}
