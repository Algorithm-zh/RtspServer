#ifndef BXC_RTSPSERVER_MEDIASOURCE_H
#define BXC_RTSPSERVER_MEDIASOURCE_H
#include "../Scheduler/ThreadPool.h"
#include "../Scheduler/UsageEnvironment.h"
#include <mutex>
#include <queue>
#include <stdint.h>

#define FRAME_MAX_SIZE (1024 * 200)
#define DEFAULT_FRAME_NUM 4

class MediaFrame {

public:
  MediaFrame()
      : temp(new uint8_t[FRAME_MAX_SIZE]),

        mBuf(nullptr), mSize(0) {}
  ~MediaFrame() { delete[] temp; }

  uint8_t *temp; // 容器
  // temp存数据，mBuf作为指针在数据中跳转，方便操纵数据
  uint8_t *mBuf; // 引用容器
  // size存储的是纯数据的大小
  int mSize;
};

class MediaSource {

public:
  explicit MediaSource(UsageEnvironment *env);
  virtual ~MediaSource();

  MediaFrame *getFrameFromOutputQueue();        // 从输出队列获取帧
  void putFrameToInputQueue(MediaFrame *frame); // 把帧送入输入队列
  int getFps() const { return mFps; }
  std::string getSourceName() { return mSourceName; }

private:
  static void taskCallback(void *arg);

protected:
  virtual void handleTask() = 0;
  void setFps(int fps) { mFps = fps; }

protected:
  UsageEnvironment *mEnv;
  MediaFrame mFrames[DEFAULT_FRAME_NUM];
  std::queue<MediaFrame *> mFrameInputQueue;
  std::queue<MediaFrame *> mFrameOutputQueue;

  std::mutex mMtx;
  ThreadPool::Task mTask;
  int mFps;
  std::string mSourceName;
};
#endif // BXC_RTSPSERVER_MEDIASOURCE_H
