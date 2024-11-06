#include "MediaSession.h"
#include "../Base/Log.h"
#include <algorithm>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

MediaSession *MediaSession::createNew(std::string sessionName) {
  return new MediaSession(sessionName);
}

MediaSession::MediaSession(const std::string &sessionName)
    : mSessionName(sessionName), mIsStartMulticast(false) {

  LOGI("MediaSession() name=%s", sessionName.data());

  mTracks[0].mTrackId = TrackId0;
  mTracks[0].mIsAlive = false;

  mTracks[1].mTrackId = TrackId1;
  mTracks[1].mIsAlive = false;

  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    mMulticastRtpInstances[i] = NULL;
    mMulticastRtcpInstances[i] = NULL;
  }
}

MediaSession::~MediaSession() {
  LOGI("~MediaSession()");
  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    if (mMulticastRtpInstances[i]) {
      this->removeRtpInstance(mMulticastRtpInstances[i]);
      delete mMulticastRtpInstances[i];
      //            Delete::release(mMulticastRtpInstances[i]);
    }

    if (mMulticastRtcpInstances[i]) {
      // Delete::release(mMulticastRtcpInstances[i]);
      delete mMulticastRtcpInstances[i];
    }
  }
  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    if (mTracks[i].mIsAlive) {
      Sink *sink = mTracks[i].mSink;
      delete sink;
    }
  }
}

// SDP就是描述会话信息的，双方通过sdp了解协议、通信方式、时间戳之类的内容
std::string MediaSession::generateSDPDescription() {
  // 如果已经生成了SDP描述，直接返回
  if (!mSdp.empty()) {
    return mSdp;
  }

  // 设置默认IP地址
  std::string ip = "0.0.0.0";
  char buf[2048] = {0}; // 初始化缓冲区

  // 构建SDP头部信息
  snprintf(buf, sizeof(buf),
           "v=0\r\n"                  // SDP版本
           "o=- 9%ld 1 IN IP4 %s\r\n" // 会话发起者信息
           "t=0 0\r\n"                // 时间戳
           "a=control:*\r\n"          // 控制信息
           "a=type:broadcast\r\n",    // 类型信息
           (long)time(NULL), ip.c_str());

  // 如果启用了多播，添加RTCP单播反射属性
  // 多播适合网络直播、在线会议，单播适合一对一的场景，因为单播每个接收方都需要一个数据流，网络宽带消耗大
  if (isStartMulticast()) {
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
             "a=rtcp-unicast: reflection\r\n");
  }

  // 遍历所有媒体轨道
  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    uint16_t port = 0;

    // 如果当前轨道未激活，跳过
    if (mTracks[i].mIsAlive != true) {
      continue;
    }

    // 如果启用了多播，获取多播目标RTP端口
    if (isStartMulticast()) {
      port = getMulticastDestRtpPort((TrackId)i);
    }

    // 添加媒体描述
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s\r\n",
             mTracks[i].mSink->getMediaDescription(port).c_str());

    // 添加连接信息
    if (isStartMulticast()) {
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
               "c=IN IP4 %s/255\r\n", getMulticastDestAddr().c_str());
    } else {
      snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
               "c=IN IP4 0.0.0.0\r\n");
    }

    // 添加媒体属性
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%s\r\n",
             mTracks[i].mSink->getAttribute().c_str());

    // 添加控制信息
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf),
             "a=control:track%d\r\n", mTracks[i].mTrackId);
  }

  // 将生成的SDP描述保存到成员变量并返回
  mSdp = buf;
  return mSdp;
}

MediaSession::Track *MediaSession::getTrack(MediaSession::TrackId trackId) {
  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    if (mTracks[i].mTrackId == trackId)
      return &mTracks[i];
  }

  return NULL;
}

bool MediaSession::addSink(MediaSession::TrackId trackId, Sink *sink) {
  Track *track = getTrack(trackId);

  if (!track)
    return false;

  track->mSink = sink;
  track->mIsAlive = true;

  sink->setSessionCb(MediaSession::sendPacketCallback, this, track);
  return true;
}

bool MediaSession::addRtpInstance(MediaSession::TrackId trackId,
                                  RtpInstance *rtpInstance) {
  Track *track = getTrack(trackId);
  if (!track || track->mIsAlive != true)
    return false;

  track->mRtpInstances.push_back(rtpInstance);

  return true;
}

bool MediaSession::removeRtpInstance(RtpInstance *rtpInstance) {
  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    if (mTracks[i].mIsAlive == false)
      continue;

    std::list<RtpInstance *>::iterator it =
        std::find(mTracks[i].mRtpInstances.begin(),
                  mTracks[i].mRtpInstances.end(), rtpInstance);
    if (it == mTracks[i].mRtpInstances.end())
      continue;

    mTracks[i].mRtpInstances.erase(it);
    return true;
  }

  return false;
}

void MediaSession::sendPacketCallback(void *arg1, void *arg2, void *packet,
                                      Sink::PacketType packetType) {
  RtpPacket *rtpPacket = (RtpPacket *)packet;

  MediaSession *session = (MediaSession *)arg1;
  MediaSession::Track *track = (MediaSession::Track *)arg2;

  session->handleSendRtpPacket(track, rtpPacket);
}

void MediaSession::handleSendRtpPacket(MediaSession::Track *track,
                                       RtpPacket *rtpPacket) {
  //    LOGI("");

  std::list<RtpInstance *>::iterator it;

  for (it = track->mRtpInstances.begin(); it != track->mRtpInstances.end();
       ++it) {
    RtpInstance *rtpInstance = *it;
    if (rtpInstance->alive()) {
      rtpInstance->send(rtpPacket);
    }
  }
}

bool MediaSession::startMulticast() {
  // 随机生成多播地址
  struct sockaddr_in addr = {0};
  uint32_t range = 0xE8FFFFFF - 0xE8000100;
  addr.sin_addr.s_addr = htonl(0xE8000100 + (rand()) % range);
  mMulticastAddr = inet_ntoa(addr.sin_addr);

  int rtpSockfd1, rtcpSockfd1;
  int rtpSockfd2, rtcpSockfd2;
  uint16_t rtpPort1, rtcpPort1;
  uint16_t rtpPort2, rtcpPort2;
  bool ret;

  rtpSockfd1 = sockets::createUdpSock();
  assert(rtpSockfd1 > 0);

  rtpSockfd2 = sockets::createUdpSock();
  assert(rtpSockfd2 > 0);

  rtcpSockfd1 = sockets::createUdpSock();
  assert(rtcpSockfd1 > 0);

  rtcpSockfd2 = sockets::createUdpSock();
  assert(rtcpSockfd2 > 0);

  uint16_t port = rand() & 0xfffe;
  if (port < 10000)
    port += 10000;

  rtpPort1 = port;
  rtcpPort1 = port + 1;
  rtpPort2 = rtcpPort1 + 1;
  rtcpPort2 = rtpPort2 + 1;

  mMulticastRtpInstances[TrackId0] =
      RtpInstance::createNewOverUdp(rtpSockfd1, 0, mMulticastAddr, rtpPort1);
  mMulticastRtpInstances[TrackId1] =
      RtpInstance::createNewOverUdp(rtpSockfd2, 0, mMulticastAddr, rtpPort2);
  mMulticastRtcpInstances[TrackId0] =
      RtcpInstance::createNew(rtcpSockfd1, 0, mMulticastAddr, rtcpPort1);
  mMulticastRtcpInstances[TrackId1] =
      RtcpInstance::createNew(rtcpSockfd2, 0, mMulticastAddr, rtcpPort2);

  this->addRtpInstance(TrackId0, mMulticastRtpInstances[TrackId0]);
  this->addRtpInstance(TrackId1, mMulticastRtpInstances[TrackId1]);
  mMulticastRtpInstances[TrackId0]->setAlive(true);
  mMulticastRtpInstances[TrackId1]->setAlive(true);

  mIsStartMulticast = true;

  return true;
}

bool MediaSession::isStartMulticast() { return mIsStartMulticast; }

uint16_t MediaSession::getMulticastDestRtpPort(TrackId trackId) {
  if (trackId > TrackId1 || !mMulticastRtpInstances[trackId])
    return -1;

  return mMulticastRtpInstances[trackId]->getPeerPort();
}
