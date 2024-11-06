#include "RtspConnection.h"
#include "../Base/Log.h"
#include "../Base/Version.h"
#include "MediaSession.h"
#include "MediaSessionManager.h"
#include "Rtp.h"
#include "RtspServer.h"
#include <stdio.h>
#include <string.h>

static void getPeerIp(int fd, std::string &ip) {
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr *)&addr, &addrlen);
  ip = inet_ntoa(addr.sin_addr);
}

RtspConnection *RtspConnection::createNew(RtspServer *rtspServer,
                                          int clientFd) {
  return new RtspConnection(rtspServer, clientFd);
  //    return New<RtspConnection>::allocate(rtspServer, clientFd);
}

RtspConnection::RtspConnection(RtspServer *rtspServer, int clientFd)
    : TcpConnection(rtspServer->env(), clientFd), mRtspServer(rtspServer),
      mMethod(RtspConnection::Method::NONE),
      mTrackId(MediaSession::TrackId::TrackIdNone), mSessionId(rand()),
      mIsRtpOverTcp(false), mStreamPrefix("track") {
  LOGI("RtspConnection() mClientFd=%d", mClientFd);

  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    mRtpInstances[i] = NULL;
    mRtcpInstances[i] = NULL;
  }
  getPeerIp(clientFd, mPeerIp);
}

RtspConnection::~RtspConnection() {
  LOGI("~RtspConnection() mClientFd=%d", mClientFd);
  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    if (mRtpInstances[i]) {

      MediaSession *session = mRtspServer->mSessMgr->getSession(mSuffix);

      if (!session) {
        session->removeRtpInstance(mRtpInstances[i]);
      }
      delete mRtpInstances[i];
    }

    if (mRtcpInstances[i]) {
      delete mRtcpInstances[i];
    }
  }
}

void RtspConnection::handleReadBytes() {

  // 是否通过tcp进行连接传输
  if (mIsRtpOverTcp) {
    // RTP over TCP 数据包的前缀字符通常是 $，这在 RTSP 协议规范中有明确规定
    if (mInputBuffer.peek()[0] == '$') {
      // 就是给RTPHeader和RTCPHeader赋值
      handleRtpOverTcp();
      return;
    }
  }

  // 解析判断消息是否正确
  if (!parseRequest()) {
    LOGE("parseRequest err");
    goto disConnect;
  }
  // 往缓冲区写回复消息并调用发送函数
  switch (mMethod) {
  case OPTIONS:
    if (!handleCmdOption())
      goto disConnect;
    break;
  case DESCRIBE:
    if (!handleCmdDescribe())
      goto disConnect;
    break;
  case SETUP:
    if (!handleCmdSetup())
      goto disConnect;
    break;
  case PLAY:
    if (!handleCmdPlay())
      goto disConnect;
    break;
  case TEARDOWN:
    if (!handleCmdTeardown())
      goto disConnect;
    break;

  default:
    goto disConnect;
  }

  return;
disConnect:
  handleDisConnect();
}

bool RtspConnection::parseRequest() {
  // 解析第一行
  const char *crlf = mInputBuffer.findCRLF();
  if (crlf == NULL) {
    // 如果没有找到 CRLF 序列，说明请求不完整，清空缓冲区并返回 false
    mInputBuffer.retrieveAll();
    return false;
  }
  // 解析请求的方法是什么以及ip、port和suffix
  bool ret = parseRequest1(mInputBuffer.peek(), crlf);
  if (ret == false) {
    // 如果解析第一行失败，清空缓冲区并返回 false
    mInputBuffer.retrieveAll();
    return false;
  } else {
    // 解析成功，移除已解析的第一行
    mInputBuffer.retrieveUntil(crlf + 2);
  }

  // 解析最后的序列
  crlf = mInputBuffer.findLastCrlf();
  if (crlf == NULL) {
    // 如果没有找到最后一个 CRLF 序列，说明请求不完整，清空缓冲区并返回 false
    mInputBuffer.retrieveAll();
    return false;
  }
  // 判断请求的方法和消息是否含正确信息并获取一些重要头部信息
  ret = parseRequest2(mInputBuffer.peek(), crlf);
  if (ret == false) {
    // 如果解析剩余行失败，清空缓冲区并返回 false
    mInputBuffer.retrieveAll();
    return false;
  } else {
    // 解析成功，移除已解析的剩余行
    mInputBuffer.retrieveUntil(crlf + 2);
    return true;
  }
}

// 解析请求的方法是什么以及url和suffix
bool RtspConnection::parseRequest1(const char *begin, const char *end) {
  std::string message(begin, end);
  char method[64] = {0};
  char url[512] = {0};
  char version[64] = {0};

  if (sscanf(message.c_str(), "%s %s %s", method, url, version) != 3) {
    return false;
  }

  if (!strcmp(method, "OPTIONS")) {
    mMethod = OPTIONS;
  } else if (!strcmp(method, "DESCRIBE")) {
    mMethod = DESCRIBE;
  } else if (!strcmp(method, "SETUP")) {
    mMethod = SETUP;
  } else if (!strcmp(method, "PLAY")) {
    mMethod = PLAY;
  } else if (!strcmp(method, "TEARDOWN")) {
    mMethod = TEARDOWN;
  } else {
    mMethod = NONE;
    return false;
  }
  if (strncmp(url, "rtsp://", 7) != 0) {
    return false;
  }
  uint16_t port = 0;
  char ip[64] = {0};
  char suffix[64] = {0};

  //  %[^:]：匹配一个不包含 : 的字符串，存储到 ip 中。例如，"192.168.1.1"。
  // :%hu：匹配一个以 : 开头的无符号短整数，存储到 port 中。例如，8554。
  // /%s：匹配一个以 / 开头的字符串，存储到 suffix 中。例如，"stream"。
  if (sscanf(url + 7, "%[^:]:%hu/%s", ip, &port, suffix) == 3) {

  } else if (sscanf(url + 7, "%[^/]/%s", ip, suffix) == 2) {
    port = 554; // 如果rtsp请求地址中无端口，默认获取的端口为：554
  } else {
    return false;
  }

  mUrl = url;
  mSuffix = suffix;

  return true;
}

// 仅仅判断方法和消息是否有效
bool RtspConnection::parseRequest2(const char *begin, const char *end) {
  std::string message(begin, end);

  if (!parseCSeq(message)) {
    return false;
  }
  if (mMethod == OPTIONS) {
    return true;
  } else if (mMethod == DESCRIBE) {
    return parseDescribe(message);
  } else if (mMethod == SETUP) {
    return parseSetup(message);
  } else if (mMethod == PLAY) {
    return parsePlay(message);
  } else if (mMethod == TEARDOWN) {
    return true;
  } else {
    return false;
  }
}

// 解析 CSeq 字段是为了确保请求和响应的匹配和顺序，管理会话状态，以及符合 RTSP
// 协议规范。通过解析 CSeq，可以确保 RTSP 通信的可靠性和正确性。
bool RtspConnection::parseCSeq(std::string &message) {
  std::size_t pos = message.find("CSeq");
  if (pos != std::string::npos) {
    uint32_t cseq = 0;
    sscanf(message.c_str() + pos, "%*[^:]: %u", &cseq);
    mCSeq = cseq;
    return true;
  }

  return false;
}

bool RtspConnection::parseDescribe(std::string &message) {
  if ((message.rfind("Accept") == std::string::npos) ||
      (message.rfind("sdp") == std::string::npos)) {
    return false;
  }

  return true;
}

bool RtspConnection::parseSetup(std::string &message) {
  mTrackId = MediaSession::TrackIdNone;
  std::size_t pos;

  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; i++) {
    pos = mUrl.find(mStreamPrefix + std::to_string(i));
    if (pos != std::string::npos) {
      if (i == 0) {
        mTrackId = MediaSession::TrackId0;
      } else if (i == 1) {
        mTrackId = MediaSession::TrackId1;
      }
    }
  }

  if (mTrackId == MediaSession::TrackIdNone) {
    return false;
  }

  pos = message.find("Transport");
  if (pos != std::string::npos) {
    if ((pos = message.find("RTP/AVP/TCP")) != std::string::npos) {
      uint8_t rtpChannel, rtcpChannel;
      mIsRtpOverTcp = true;

      if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hhu-%hhu",
                 &rtpChannel, &rtcpChannel) != 2) {
        return false;
      }

      mRtpChannel = rtpChannel;

      return true;
    } else if ((pos = message.find("RTP/AVP")) != std::string::npos) {
      uint16_t rtpPort = 0, rtcpPort = 0;
      if (((message.find("unicast", pos)) != std::string::npos)) {
        if (sscanf(message.c_str() + pos, "%*[^;];%*[^;];%*[^=]=%hu-%hu",
                   &rtpPort, &rtcpPort) != 2) {
          return false;
        }
      } else if ((message.find("multicast", pos)) != std::string::npos) {
        return true;
      } else
        return false;

      mPeerRtpPort = rtpPort;
      mPeerRtcpPort = rtcpPort;
    } else {
      return false;
    }

    return true;
  }

  return false;
}

bool RtspConnection::parsePlay(std::string &message) {
  std::size_t pos = message.find("Session");
  if (pos != std::string::npos) {
    uint32_t sessionId = 0;
    // 从 "Session" 关键字的位置开始解析，提取会话 ID
    if (sscanf(message.c_str() + pos, "%*[^:]: %u", &sessionId) != 1)
      return false;
    return true;
  }

  return false;
}

bool RtspConnection::handleCmdOption() {

  snprintf(mBuffer, sizeof(mBuffer),
           "RTSP/1.0 200 OK\r\n"
           "CSeq: %u\r\n"
           "Public: DESCRIBE, ANNOUNCE, SETUP, PLAY, RECORD, PAUSE, "
           "GET_PARAMETER, TEARDOWN\r\n"
           "Server: %s\r\n"
           "\r\n",
           mCSeq, PROJECT_VERSION);

  if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
    return false;

  return true;
}

bool RtspConnection::handleCmdDescribe() {
  MediaSession *session = mRtspServer->mSessMgr->getSession(mSuffix);

  if (!session) {
    LOGE("can't find session:%s", mSuffix.c_str());
    return false;
  }
  std::string sdp = session->generateSDPDescription();

  memset((void *)mBuffer, 0, sizeof(mBuffer));
  snprintf((char *)mBuffer, sizeof(mBuffer),
           "RTSP/1.0 200 OK\r\n"
           "CSeq: %u\r\n"
           "Content-Length: %u\r\n"
           "Content-Type: application/sdp\r\n"
           "\r\n"
           "%s",
           mCSeq, (unsigned int)sdp.size(), sdp.c_str());

  if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
    return false;

  return true;
}

bool RtspConnection::handleCmdSetup() {
  char sessionName[100];
  if (sscanf(mSuffix.c_str(), "%[^/]/", sessionName) != 1) {
    return false;
  }
  MediaSession *session = mRtspServer->mSessMgr->getSession(sessionName);
  if (!session) {
    LOGE("can't find session:%s", sessionName);
    return false;
  }

  if (mTrackId >= MEDIA_MAX_TRACK_NUM || mRtpInstances[mTrackId] ||
      mRtcpInstances[mTrackId]) {
    return false;
  }

  if (session->isStartMulticast()) {
    snprintf((char *)mBuffer, sizeof(mBuffer),
             "RTSP/1.0 200 OK\r\n"
             "CSeq: %d\r\n"
             "Transport: RTP/AVP;multicast;"
             "destination=%s;source=%s;port=%d-%d;ttl=255\r\n"
             "Session: %08x\r\n"
             "\r\n",
             mCSeq, session->getMulticastDestAddr().c_str(),
             sockets::getLocalIp().c_str(),
             session->getMulticastDestRtpPort(mTrackId),
             session->getMulticastDestRtpPort(mTrackId) + 1, mSessionId);
  } else {

    if (mIsRtpOverTcp) {
      // 创建rtp over tcp
      // 即创建RTPInstances
      createRtpOverTcp(mTrackId, mClientFd, mRtpChannel);
      mRtpInstances[mTrackId]->setSessionId(mSessionId);

      session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);

      snprintf((char *)mBuffer, sizeof(mBuffer),
               "RTSP/1.0 200 OK\r\n"
               "CSeq: %d\r\n"
               "Server: %s\r\n"
               "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n"
               "Session: %08x\r\n"
               "\r\n",
               mCSeq, PROJECT_VERSION, mRtpChannel, mRtpChannel + 1,
               mSessionId);
    } else {
      // 创建 rtp over udp
      if (createRtpRtcpOverUdp(mTrackId, mPeerIp, mPeerRtpPort,
                               mPeerRtcpPort) != true) {
        LOGE("failed to createRtpRtcpOverUdp");
        return false;
      }

      mRtpInstances[mTrackId]->setSessionId(mSessionId);
      mRtcpInstances[mTrackId]->setSessionId(mSessionId);

      session->addRtpInstance(mTrackId, mRtpInstances[mTrackId]);

      snprintf((char *)mBuffer, sizeof(mBuffer),
               "RTSP/1.0 200 OK\r\n"
               "CSeq: %u\r\n"
               "Server: %s\r\n"
               "Transport: "
               "RTP/AVP;unicast;client_port=%hu-%hu;server_port=%hu-%hu\r\n"
               "Session: %08x\r\n"
               "\r\n",
               mCSeq, PROJECT_VERSION, mPeerRtpPort, mPeerRtcpPort,
               mRtpInstances[mTrackId]->getLocalPort(),
               mRtcpInstances[mTrackId]->getLocalPort(), mSessionId);
    }
  }

  if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
    return false;

  return true;
}

bool RtspConnection::handleCmdPlay() {
  snprintf((char *)mBuffer, sizeof(mBuffer),
           "RTSP/1.0 200 OK\r\n"
           "CSeq: %d\r\n"
           "Server: %s\r\n"
           "Range: npt=0.000-\r\n"
           "Session: %08x; timeout=60\r\n"
           "\r\n",
           mCSeq, PROJECT_VERSION, mSessionId);

  if (sendMessage(mBuffer, strlen(mBuffer)) < 0)
    return false;

  for (int i = 0; i < MEDIA_MAX_TRACK_NUM; ++i) {
    if (mRtpInstances[i]) {
      mRtpInstances[i]->setAlive(true);
    }

    if (mRtcpInstances[i]) {
      mRtcpInstances[i]->setAlive(true);
    }
  }

  return true;
}

// TEARDOWN 消息用于终止 RTSP 会话并释放相关资源，服务器通过发送 200 OK
// 响应来确认请求已成功处理TEARDOWN 消息用于终止 RTSP
// 会话并释放相关资源，服务器通过发送 200 OK 响应来确认请求已成功处理
bool RtspConnection::handleCmdTeardown() {
  snprintf((char *)mBuffer, sizeof(mBuffer),
           "RTSP/1.0 200 OK\r\n"
           "CSeq: %d\r\n"
           "Server: %s\r\n"
           "\r\n",
           mCSeq, PROJECT_VERSION);

  if (sendMessage(mBuffer, strlen(mBuffer)) < 0) {
    return false;
  }

  return true;
}

int RtspConnection::sendMessage(void *buf, int size) {
  LOGI("%s", buf);
  int ret;

  mOutBuffer.append(buf, size);
  ret = mOutBuffer.write(mClientFd);
  mOutBuffer.retrieveAll();

  return ret;
}

int RtspConnection::sendMessage() {
  int ret = mOutBuffer.write(mClientFd);
  mOutBuffer.retrieveAll();
  return ret;
}

// 因为udp使用不同协议使用不同通道，所以操作比较麻烦
bool RtspConnection::createRtpRtcpOverUdp(MediaSession::TrackId trackId,
                                          std::string peerIp,
                                          uint16_t peerRtpPort,
                                          uint16_t peerRtcpPort) {
  int rtpSockfd, rtcpSockfd;
  int16_t rtpPort, rtcpPort;
  bool ret;

  if (mRtpInstances[trackId] || mRtcpInstances[trackId])
    return false;

  int i;
  for (i = 0; i < 10; ++i) { // 重试10次
    rtpSockfd = sockets::createUdpSock();
    if (rtpSockfd < 0) {
      return false;
    }

    rtcpSockfd = sockets::createUdpSock();
    if (rtcpSockfd < 0) {
      sockets::close(rtpSockfd);
      return false;
    }

    uint16_t port = rand() & 0xfffe;
    if (port < 10000)
      port += 10000;

    rtpPort = port;
    rtcpPort = port + 1;

    ret = sockets::bind(rtpSockfd, "0.0.0.0", rtpPort);
    if (ret != true) {
      sockets::close(rtpSockfd);
      sockets::close(rtcpSockfd);
      continue;
    }

    ret = sockets::bind(rtcpSockfd, "0.0.0.0", rtcpPort);
    if (ret != true) {
      sockets::close(rtpSockfd);
      sockets::close(rtcpSockfd);
      continue;
    }

    break;
  }

  if (i == 10)
    return false;

  mRtpInstances[trackId] =
      RtpInstance::createNewOverUdp(rtpSockfd, rtpPort, peerIp, peerRtpPort);
  mRtcpInstances[trackId] =
      RtcpInstance::createNew(rtcpSockfd, rtcpPort, peerIp, peerRtcpPort);

  return true;
}

bool RtspConnection::createRtpOverTcp(MediaSession::TrackId trackId, int sockfd,
                                      uint8_t rtpChannel) {
  mRtpInstances[trackId] = RtpInstance::createNewOverTcp(sockfd, rtpChannel);

  return true;
}

void RtspConnection::handleRtpOverTcp() {
  int num = 0; // 初始化计数器，用于记录处理的 RTP/RTCP 包数量

  while (true) {
    num += 1; // 每处理一个 RTP/RTCP 包，计数器加一

    // 详细解释:https://www.cnblogs.com/iFrank/p/15434438.html
    //  TCP传输独有的三个字段分别为magic(标识符，即$,和rtsp区分)
    //  channel(区分RTP和RTCP) Length（占2个字节）位于数据包的开头
    uint8_t *buf = (uint8_t *)mInputBuffer.peek(); // 获取输入缓冲区的指针
    uint8_t rtpChannel = buf[1];                   // 获取 RTP 通道号
    int16_t rtpSize = (buf[2] << 8) | buf[3];      // 获取 RTP 数据包的大小

    int16_t bufSize = 4 + rtpSize; // 计算整个 RTP/RTCP 包的总大小（包括头部）

    if (mInputBuffer.readableBytes() < bufSize) {
      // 如果缓存数据小于一个 RTP 数据包的长度，返回
      return;
    } else {
      // 根据 RTP 通道号处理不同的 RTP/RTCP 包
      // 一般情况RTP通道编号是偶数，RTCP是奇数
      if (0x00 == rtpChannel) {
        RtpHeader rtpHeader;
        // 就是给rtpHeader结构体赋值
        parseRtpHeader(buf + 4, &rtpHeader);      // 解析 RTP 头部
        LOGI("num=%d, rtpSize=%d", num, rtpSize); // 记录日志
      } else if (0x01 == rtpChannel) {
        RtcpHeader rtcpHeader;
        parseRtcpHeader(buf + 4, &rtcpHeader); // 解析 RTCP 头部
        LOGI("num=%d, rtcpHeader.packetType=%d, rtpSize=%d", num,
             rtcpHeader.packetType, rtpSize); // 记录日志
      } else if (0x02 == rtpChannel) {
        RtpHeader rtpHeader;
        parseRtpHeader(buf + 4, &rtpHeader);      // 解析 RTP 头部
        LOGI("num=%d, rtpSize=%d", num, rtpSize); // 记录日志
      } else if (0x03 == rtpChannel) {
        RtcpHeader rtcpHeader;
        parseRtcpHeader(buf + 4, &rtcpHeader); // 解析 RTCP 头部
        LOGI("num=%d, rtcpHeader.packetType=%d, rtpSize=%d", num,
             rtcpHeader.packetType, rtpSize); // 记录日志
      }

      mInputBuffer.retrieve(bufSize); // 从输入缓冲区中移除已处理的 RTP/RTCP 包
    }
  }
}
