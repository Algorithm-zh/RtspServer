从零开始写一个RTSP服务器系列

[★我的开源项目-RtspServer][-RtspServer]

[从零开始写一个RTSP服务器（一）RTSP协议讲解][RTSP_RTSP]

[从零开始写一个RTSP服务器（二）RTSP协议的实现][RTSP_RTSP 1]

[从零开始写一个RTSP服务器（三）RTP传输H.264][RTSP_RTP_H.264]

[从零开始写一个RTSP服务器（四）一个传输H.264的RTSP服务器][RTSP_H.264_RTSP]

[从零开始写一个RTSP服务器（五）RTP传输AAC][RTSP_RTP_AAC]

[从零开始写一个RTSP服务器（六）一个传输AAC的RTSP服务器][RTSP_AAC_RTSP]

[从零开始写一个RTSP服务器（七）多播传输RTP包][RTSP_RTP]

[从零开始写一个RTSP服务器（八）一个多播的RTSP服务器][RTSP_RTSP 2]

[从零开始写一个RTSP服务器（九）一个RTP OVER RTSP/TCP的RTSP服务器][RTSP_RTP OVER RTSP_TCP_RTSP]

## 从零开始写一个RTSP服务器（九）一个RTP OVER RTSP/TCP的RTSP服务器 

#### 文章目录 

 *  [从零开始写一个RTSP服务器（九）一个RTP OVER RTSP/TCP的RTSP服务器][RTSP_RTP OVER RTSP_TCP_RTSP 1]
 *   *  [一、RTP OVER RTSP(TCP)的实现][RTP OVER RTSP_TCP]
     *   *  [1.1 发送RTP包方式][1.1 _RTP]
         *  [1.2 如何区分RTSP、RTP、RTCP？][1.2 _RTSP_RTP_RTCP]
     *  [二、RTP OVER RTSP(TCP)的RTSP交互过程][RTP OVER RTSP_TCP_RTSP]
     *   *  [OPTIONS][]
         *  [DESCRIBE][]
         *  [SETUP][]
         *  [PLAY][]
     *  [三、实现过程][Link 1]
     *   *  [3.1 RTP发包][3.1 RTP]
         *  [3.2 服务器的实现][3.2]
         *   *  [3.2.1 创建socket套接字][3.2.1 _socket]
             *  [3.2.2 接收客户端连接][3.2.2]
             *  [3.2.3 解析请求][3.2.3]
             *  [3.2.4 处理请求][3.2.4]
             *  [3.2.5 发送RTP包][3.2.5 _RTP]
         *  [3.3 源码][3.3]
         *   *  [rtsp\_server.c][rtsp_server.c]
             *  [tcp\_rtp.h][tcp_rtp.h]
             *  [tcp\_rtp.c][tcp_rtp.c]
     *  [四、测试][Link 2]

### 一、RTP OVER RTSP(TCP)的实现 

#### 1.1 发送RTP包方式 

对于RTP OVER UDP 的实现，我们使用TCP连接来发送RTSP交互，然后创建新的UDP套接字来发送RTP包

对于RTP OVER RTSP(TCP)来说，我们会复用使用原先发送RTSP的socket来发送RTP包

#### 1.2 如何区分RTSP、RTP、RTCP？ 

如上面所说，我们复用发送RTSP交互的socket来发送RTP包和RTCP信息，那么对于客户端来说，如何区分这三种数据呢？

我们将这三个分为两类，一类是RTSP，一类是RTP、RTCP

发送RTSP信息的情况没有变化，还是更以前一样的方式

发送RTP、RTCP包，在每个包前面都加上四个字节

<table> 
 <thead> 
  <tr> 
   <th>字节</th> 
   <th>描述</th> 
  </tr> 
 </thead> 
 <tbody> 
  <tr> 
   <td>第一个字节</td> 
   <td>‘$’，标识符，用于与RTSP区分</td> 
  </tr> 
  <tr> 
   <td>第二个字节</td> 
   <td>channel，用于区分RTP和RTCP</td> 
  </tr> 
  <tr> 
   <td>第三和第四个字节</td> 
   <td>RTP包的大小</td> 
  </tr> 
 </tbody> 
</table>

由此我们可知，第一个字节’$'用于与RTSP区分，第二个字节用于区分RTP和RTCP

RTP和RTCP的channel是在RTSP的SETUP过程中，客户端发送给服务端的

所以现在RTP的打包方式要在之前的每个RTP包前面加上四个字节，如下所示

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/37651831bf6d9e7e13ea82d15da1af10.png)

### 二、RTP OVER RTSP(TCP)的RTSP交互过程 

#### OPTIONS 

 *  C–>S
    
    ```java
    OPTIONS rtsp://127.0.0.1:8554/live RTSP/1.0\r\n
    CSeq: 2\r\n
    \r\n
    ```
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 2\r\n
    Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n
    \r\n
    ```

#### DESCRIBE 

 *  C–>S
    
    ```java
    DESCRIBE rtsp://127.0.0.1:8554/live RTSP/1.0\r\n
    CSeq: 3\r\n
    Accept: application/sdp\r\n
    \r\n
    ```
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 3\r\n
    Content-length: 146
    Content-type: application/sdp
    \r\n
    v=0
    o=- 91565667683 1 IN IP4 127.0.0.1
    t=0 0
    a=control:*
    m=video 0 RTP/AVP 96
    a=rtpmap:96 H264/90000
    a=framerate:25
    a=control:track0
    ```

#### SETUP 

 *  C–>S
    
    ```java
    SETUP rtsp://127.0.0.1:8554/live/track0 RTSP/1.0\r\n
    CSeq: 4\r\n
    Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n
    \r\n
    ```
    
    `RTP/AVP/TCP`表示使用RTP OVER TCP，`interleaved=0-1`表示这个会话连接的RTP channel为0，RTCP channel为1
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 4\r\n
    Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n
    Session: 327b23c6\r\n
    \r\n
    ```

#### PLAY 

 *  C–>S
    
    ```java
    PLAY rtsp://127.0.0.1:8554/live RTSP/1.0\r\n
    CSeq: 5\r\n
    Session: 327b23c6\r\n
    Range: npt=0.000-\r\n
    \r\n
    ```
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 5\r\n
    Range: npt=0.000-\r\n
    Session: 327b23c6; timeout=60\r\n
    \r\n
    ```

### 三、实现过程 

#### 3.1 RTP发包 

经过上面的介绍，我们知道RTP OVER TCP和RTP OVER UDP的RTP发包方式是不同的，RTP OVER TCP需要在整一个RTP包前面加上四个字节，为此我修改了RTP发包部分

 *  RTP Packet 结构体
    
    ```java
    /*
     * 作者：_JT_
     * 博客：https://blog.csdn.net/weixin_42462202
     */
     
    struct RtpPacket
    {
        char header[4];
        struct RtpHeader rtpHeader;
        uint8_t payload[0];
    };
    ```
    
    header：前四个字节
    
    rtpHeader：RTP包头部
    
    payload：RTP包载荷
 *  RTP的发包函数修改
    
    每次发包前都需要添加四个字节的头，并且通过tcp发送
    
    ```java
    /*
     * 作者：_JT_
     * 博客：https://blog.csdn.net/weixin_42462202
     */
     
    rtpSendPacket()
    {
                  
         
           
           
        ...
        
        rtpPacket->header[0] = '$';
        rtpPacket->header[1] = rtpChannel;
        rtpPacket->header[2] = (size & 0xFF00 ) >> 8;
        rtpPacket->header[3] = size & 0xFF;
        
        ...
        send(...);
        ...
    }
    ```

#### 3.2 服务器的实现 

下面开始介绍RTP OVER TCP服务器的实现过程

##### 3.2.1 创建socket套接字 

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

main()
{
            
   
     
     
    serverSockfd = createTcpSocket();
    bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT);
    listen(serverSockfd, 10);
    ...
    while(1)
    {
            
   
     
     
        ...
    }
}
```

##### 3.2.2 接收客户端连接 

```java
main()
{
            
   
     
     
    ...
    while(1)
    {
            
   
     
     
        acceptClient(serverSockfd, clientIp, &clientPort);
        doClient(clientSockfd, clientIp, clientPort);
    }
}
```

接收客户端连接后，执行doClient处理客户端请求

##### 3.2.3 解析请求 

接收请求后，解析请求，先解析方法，再解析序列号，如果是SETUP，那么就将RTP通道和RTCP通道解析出来

然后处理不同的请求方法

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

doClient()
{
            
   
     
     
    while(1)
    {
            
   
     
     
        /* 接收数据 */
        recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);

        /* 解析命令 */
        sscanf(line, "%s %s %s\r\n", method, url, version);
        ...
        sscanf(line, "CSeq: %d\r\n", &cseq);
        ...
        if(!strcmp(method, "SETUP"))
            sscanf(line, "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n",
                   &rtpChannel, &rtcpChannel);

        /* 处理请求 */
        if(!strcmp(method, "OPTIONS"))
            handleCmd_OPTIONS(sBuf, cseq)
        else if(!strcmp(method, "DESCRIBE"))
            handleCmd_DESCRIBE(sBuf, cseq, url);
        else if(!strcmp(method, "SETUP"))
            handleCmd_SETUP(sBuf, cseq, rtpChannel);
        else if(!strcmp(method, "PLAY"))
            handleCmd_PLAY(sBuf, cseq);

        send(clientSockfd, sBuf, strlen(sBuf), 0);
    }
}
```

##### 3.2.4 处理请求 

 *  OPTIONS
    
    ```java
    handleCmd_OPTIONS()
    {
                  
         
           
           
        sprintf(result, "RTSP/1.0 200 OK\r\n"
                        "CSeq: %d\r\n"
                        "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
                        "\r\n",
                        cseq);
    }
    ```
 *  DESCRIBE
    
    ```java
    handleCmd_DESCRIBE()
    {
                  
         
           
           
        sprintf(sdp, "v=0\r\n"
                     "o=- 9%ld 1 IN IP4 %s\r\n"
                     "t=0 0\r\n"
                     "a=control:*\r\n"
                     "m=video 0 RTP/AVP 96\r\n"
                     "a=rtpmap:96 H264/90000\r\n"
                     "a=control:track0\r\n",
                     time(NULL), localIp);
        
        sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                        "Content-Base: %s\r\n"
                        "Content-type: application/sdp\r\n"
                        "Content-length: %d\r\n\r\n"
                        "%s",
                        cseq,
                        url,
                        strlen(sdp),
                        sdp);
    }
    ```
 *  SETUP
    
    ```java
    handleCmd_SETUP()
    {
                  
         
           
           
       sprintf(result, "RTSP/1.0 200 OK\r\n"
                        "CSeq: %d\r\n"
                        "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n"
                        "Session: 66334873\r\n"
                        "\r\n",
                        cseq,
                        rtpChannel,
                        rtpChannel+1
                        );
    }
    ```
 *  PLAY
    
    ```java
    handleCmd_PLAY()
    {
                  
         
           
           
        sprintf(result, "RTSP/1.0 200 OK\r\n"
                        "CSeq: %d\r\n"
                        "Range: npt=0.000-\r\n"
                        "Session: 66334873; timeout=60\r\n\r\n",
                        cseq);
    }
    ```

##### 3.2.5 发送RTP包 

在发送完PLAY回复之后，开始发送RTP包

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

doClient()
{
            
   
     
     
    ...
    
        
	while(1)
    {
            
   
     
     
        ...
         
        send(clientSockfd, sBuf, strlen(sBuf), 0);
        
        if(!strcmp(method, "PLAY"))
        {
            
   
     
     
            while(1)
            {
            
   
     
     
                /* 获取一帧数据 */
                getFrameFromH264File(fd, frame, 500000);
                
                /* 发送RTP包 */
                rtpSendH264Frame(clientSockfd);
            }
        }
    }
}
```

#### 3.3 源码 

##### rtsp\_server.c 

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "tcp_rtp.h"

#define H264_FILE_NAME  "test.h264"
#define SERVER_PORT     8554
#define BUF_MAX_SIZE    (1024*1024)

static int createTcpSocket()
{
            
   
     
     
    int sockfd;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int createUdpSocket()
{
            
   
     
     
    int sockfd;
    int on = 1;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0)
        return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    return sockfd;
}

static int bindSocketAddr(int sockfd, const char* ip, int port)
{
            
   
     
     
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    if(bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
        return -1;

    return 0;
}

static int acceptClient(int sockfd, char* ip, int* port)
{
            
   
     
     
    int clientfd;
    socklen_t len = 0;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    len = sizeof(addr);

    clientfd = accept(sockfd, (struct sockaddr *)&addr, &len);
    if(clientfd < 0)
        return -1;
    
    strcpy(ip, inet_ntoa(addr.sin_addr));
    *port = ntohs(addr.sin_port);

    return clientfd;
}

static inline int startCode3(char* buf)
{
            
   
     
     
    if(buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
        return 1;
    else
        return 0;
}

static inline int startCode4(char* buf)
{
            
   
     
     
    if(buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
        return 1;
    else
        return 0;
}

static char* findNextStartCode(char* buf, int len)
{
            
   
     
     
    int i;

    if(len < 3)
        return NULL;

    for(i = 0; i < len-3; ++i)
    {
            
   
     
     
        if(startCode3(buf) || startCode4(buf))
            return buf;
        
        ++buf;
    }

    if(startCode3(buf))
        return buf;

    return NULL;
}

static int getFrameFromH264File(int fd, char* frame, int size)
{
            
   
     
     
    int rSize, frameSize;
    char* nextStartCode;

    if(fd < 0)
        return fd;

    rSize = read(fd, frame, size);
    if(!startCode3(frame) && !startCode4(frame))
        return -1;
    
    nextStartCode = findNextStartCode(frame+3, rSize-3);
    if(!nextStartCode)
    {
            
   
     
     
        //lseek(fd, 0, SEEK_SET);
        //frameSize = rSize;
        return -1;
    }
    else
    {
            
   
     
     
        frameSize = (nextStartCode-frame);
        lseek(fd, frameSize-rSize, SEEK_CUR);
    }

    return frameSize;
}

static int rtpSendH264Frame(int socket, int rtpChannel, struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize)
{
            
   
     
     
    uint8_t naluType; // nalu第一个字节
    int sendBytes = 0;
    int ret;

    naluType = frame[0];

    if (frameSize <= RTP_MAX_PKT_SIZE) // nalu长度小于最大包场：单一NALU单元模式
    {
            
   
     
     
        /*
         *   0 1 2 3 4 5 6 7 8 9
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         *  |F|NRI|  Type   | a single NAL unit ... |
         *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */
        memcpy(rtpPacket->payload, frame, frameSize);
        ret = rtpSendPacket(socket, rtpChannel, rtpPacket, frameSize);
        if(ret < 0)
            return -1;

        rtpPacket->rtpHeader.seq++;
        sendBytes += ret;
        if ((naluType & 0x1F) == 7 || (naluType & 0x1F) == 8) // 如果是SPS、PPS就不需要加时间戳
            goto out;
    }
    else // nalu长度小于最大包场：分片模式
    {
            
   
     
     
        /*
         *  0                   1                   2
         *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         * | FU indicator  |   FU header   |   FU payload   ...  |
         * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
         */

        /*
         *     FU Indicator
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |F|NRI|  Type   |
         *   +---------------+
         */

        /*
         *      FU Header
         *    0 1 2 3 4 5 6 7
         *   +-+-+-+-+-+-+-+-+
         *   |S|E|R|  Type   |
         *   +---------------+
         */

        int pktNum = frameSize / RTP_MAX_PKT_SIZE;       // 有几个完整的包
        int remainPktSize = frameSize % RTP_MAX_PKT_SIZE; // 剩余不完整包的大小
        int i, pos = 1;

        /* 发送完整的包 */
        for (i = 0; i < pktNum; i++)
        {
            
   
     
     
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;
            
            if (i == 0) //第一包数据
                rtpPacket->payload[1] |= 0x80; // start
            else if (remainPktSize == 0 && i == pktNum - 1) //最后一包数据
                rtpPacket->payload[1] |= 0x40; // end

            memcpy(rtpPacket->payload+2, frame+pos, RTP_MAX_PKT_SIZE);
            ret = rtpSendPacket(socket, rtpChannel, rtpPacket, RTP_MAX_PKT_SIZE+2);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
            pos += RTP_MAX_PKT_SIZE;
        }

        /* 发送剩余的数据 */
        if (remainPktSize > 0)
        {
            
   
     
     
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;
            rtpPacket->payload[1] |= 0x40; //end

            memcpy(rtpPacket->payload+2, frame+pos, remainPktSize+2);
            ret = rtpSendPacket(socket, rtpChannel, rtpPacket, remainPktSize+2);
            if(ret < 0)
                return -1;

            rtpPacket->rtpHeader.seq++;
            sendBytes += ret;
        }
    }

out:

    return sendBytes;
}

static char* getLineFromBuf(char* buf, char* line)
{
            
   
     
     
    while(*buf != '\n')
    {
            
   
     
     
        *line = *buf;
        line++;
        buf++;
    }

    *line = '\n';
    ++line;
    *line = '\0';

    ++buf;
    return buf; 
}

static int handleCmd_OPTIONS(char* result, int cseq)
{
            
   
     
     
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
                    "\r\n",
                    cseq);
                
    return 0;
}

static int handleCmd_DESCRIBE(char* result, int cseq, char* url)
{
            
   
     
     
    char sdp[500];
    char localIp[100];

    sscanf(url, "rtsp://%[^:]:", localIp);

    sprintf(sdp, "v=0\r\n"
                 "o=- 9%ld 1 IN IP4 %s\r\n"
                 "t=0 0\r\n"
                 "a=control:*\r\n"
                 "m=video 0 RTP/AVP 96\r\n"
                 "a=rtpmap:96 H264/90000\r\n"
                 "a=control:track0\r\n",
                 time(NULL), localIp);
    
    sprintf(result, "RTSP/1.0 200 OK\r\nCSeq: %d\r\n"
                    "Content-Base: %s\r\n"
                    "Content-type: application/sdp\r\n"
                    "Content-length: %d\r\n\r\n"
                    "%s",
                    cseq,
                    url,
                    strlen(sdp),
                    sdp);
    
    return 0;
}

static int handleCmd_SETUP(char* result, int cseq, uint8_t rtpChannel)
{
            
   
     
     
   sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n"
                    "Session: 66334873\r\n"
                    "\r\n",
                    cseq,
                    rtpChannel,
                    rtpChannel+1
                    );
    
    return 0;
}

static int handleCmd_PLAY(char* result, int cseq)
{
            
   
     
     
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Range: npt=0.000-\r\n"
                    "Session: 66334873; timeout=60\r\n\r\n",
                    cseq);
    
    return 0;
}

static void doClient(int clientSockfd, const char* clientIP, int clientPort)
{
            
   
     
     
    char method[40];
    char url[100];
    char version[40];
    int cseq;
    char *bufPtr;
    char* rBuf = malloc(BUF_MAX_SIZE);
    char* sBuf = malloc(BUF_MAX_SIZE);
    char line[400];
    uint8_t rtpChannel;
    uint8_t rtcpChannel;

    while(1)
    {
            
   
     
     
        int recvLen;

        recvLen = recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);
        if(recvLen <= 0)
            goto out;

        rBuf[recvLen] = '\0';
        printf("---------------C->S--------------\n");
        printf("%s", rBuf);

        /* 解析方法 */
        bufPtr = getLineFromBuf(rBuf, line);
        if(sscanf(line, "%s %s %s\r\n", method, url, version) != 3)
        {
            
   
     
     
            printf("parse err\n");
            goto out;
        }

        /* 解析序列号 */
        bufPtr = getLineFromBuf(bufPtr, line);
        if(sscanf(line, "CSeq: %d\r\n", &cseq) != 1)
        {
            
   
     
     
            printf("parse err\n");
            goto out;
        }

        /* 如果是SETUP，那么就再解析channel */
        if(!strcmp(method, "SETUP"))
        {
            
   
     
     
            while(1)
            {
            
   
     
     
                bufPtr = getLineFromBuf(bufPtr, line);
                if(!strncmp(line, "Transport:", strlen("Transport:")))
                {
            
   
     
     
                    sscanf(line, "Transport: RTP/AVP/TCP;unicast;interleaved=%hhu-%hhu\r\n",
                                    &rtpChannel, &rtcpChannel);
                    break;
                }
            }
        }

        if(!strcmp(method, "OPTIONS"))
        {
            
   
     
     
            if(handleCmd_OPTIONS(sBuf, cseq))
            {
            
   
     
     
                printf("failed to handle options\n");
                goto out;
            }
        }
        else if(!strcmp(method, "DESCRIBE"))
        {
            
   
     
     
            if(handleCmd_DESCRIBE(sBuf, cseq, url))
            {
            
   
     
     
                printf("failed to handle describe\n");
                goto out;
            }
        }
        else if(!strcmp(method, "SETUP"))
        {
            
   
     
     
            if(handleCmd_SETUP(sBuf, cseq, rtpChannel))
            {
            
   
     
     
                printf("failed to handle setup\n");
                goto out;
            }
        }
        else if(!strcmp(method, "PLAY"))
        {
            
   
     
     
            if(handleCmd_PLAY(sBuf, cseq))
            {
            
   
     
     
                printf("failed to handle play\n");
                goto out;
            }
        }
        else
        {
            
   
     
     
            goto out;
        }

        printf("---------------S->C--------------\n");
        printf("%s", sBuf);
        send(clientSockfd, sBuf, strlen(sBuf), 0);

        /* 开始播放，发送RTP包 */
        if(!strcmp(method, "PLAY"))
        {
            
   
     
     
            int frameSize, startCode;
            char* frame = malloc(500000);
            struct RtpPacket* rtpPacket = (struct RtpPacket*)malloc(500000);
            int fd = open(H264_FILE_NAME, O_RDONLY);
            assert(fd > 0);
            rtpHeaderInit(rtpPacket, 0, 0, 0, RTP_VESION, RTP_PAYLOAD_TYPE_H264, 0,
                            0, 0, 0x88923423);

            printf("start play\n");

            while (1)
            {
            
   
     
     
                frameSize = getFrameFromH264File(fd, frame, 500000);
                if(frameSize < 0)
                {
            
   
     
     
                    break;
                }

                if(startCode3(frame))
                    startCode = 3;
                else
                    startCode = 4;

                frameSize -= startCode;

                rtpSendH264Frame(clientSockfd, rtpChannel, rtpPacket, frame+startCode, frameSize);
                rtpPacket->rtpHeader.timestamp += 90000/25;

                usleep(1000*1000/25);
            }
            free(frame);
            free(rtpPacket);
            goto out;
        }

    }
out:
    printf("finish\n");
    close(clientSockfd);
    free(rBuf);
    free(sBuf);
}

int main(int argc, char* argv[])
{
            
   
     
     
    int serverSockfd;
    int ret;

    serverSockfd = createTcpSocket();
    if(serverSockfd < 0)
    {
            
   
     
     
        printf("failed to create tcp socket\n");
        return -1;
    }

    ret = bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT);
    if(ret < 0)
    {
            
   
     
     
        printf("failed to bind addr\n");
        return -1;
    }

    ret = listen(serverSockfd, 10);
    if(ret < 0)
    {
            
   
     
     
        printf("failed to listen\n");
        return -1;
    }

    printf("rtsp://127.0.0.1:%d\n", SERVER_PORT);

    while(1)
    {
            
   
     
     
        int clientSockfd;
        char clientIp[40];
        int clientPort;

        clientSockfd = acceptClient(serverSockfd, clientIp, &clientPort);
        if(clientSockfd < 0)
        {
            
   
     
     
            printf("failed to accept client\n");
            return -1;
        }

        printf("accept client;client ip:%s,client port:%d\n", clientIp, clientPort);

        doClient(clientSockfd, clientIp, clientPort);
    }

    return 0;
}
```

##### tcp\_rtp.h 

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

#ifndef _RTP_H_
#define _RTP_H_
#include <stdint.h>

#define RTP_VESION              2

#define RTP_PAYLOAD_TYPE_H264   96
#define RTP_PAYLOAD_TYPE_AAC    97

#define RTP_HEADER_SIZE         12
#define RTP_MAX_PKT_SIZE        1400

/*
 *
 *    0                   1                   2                   3
 *    7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |                           timestamp                           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   |           synchronization source (SSRC) identifier            |
 *   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *   |            contributing source (CSRC) identifiers             |
 *   :                             ....                              :
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
struct RtpHeader
{
            
   
     
     
    /* byte 0 */
    uint8_t csrcLen:4;
    uint8_t extension:1;
    uint8_t padding:1;
    uint8_t version:2;

    /* byte 1 */
    uint8_t payloadType:7;
    uint8_t marker:1;
    
    /* bytes 2,3 */
    uint16_t seq;
    
    /* bytes 4-7 */
    uint32_t timestamp;
    
    /* bytes 8-11 */
    uint32_t ssrc;
};

struct RtpPacket
{
            
   
     
     
    char header[4];
    struct RtpHeader rtpHeader;
    uint8_t payload[0];
};

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
                    uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker,
                   uint16_t seq, uint32_t timestamp, uint32_t ssrc);
int rtpSendPacket(int socket, uint8_t rtpChannel, struct RtpPacket* rtpPacket, uint32_t dataSize);

#endif //_RTP_H_
```

##### tcp\_rtp.c 

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tcp_rtp.h"

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
                    uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker,
                   uint16_t seq, uint32_t timestamp, uint32_t ssrc)
{
            
   
     
     
    rtpPacket->rtpHeader.csrcLen = csrcLen;
    rtpPacket->rtpHeader.extension = extension;
    rtpPacket->rtpHeader.padding = padding;
    rtpPacket->rtpHeader.version = version;
    rtpPacket->rtpHeader.payloadType =  payloadType;
    rtpPacket->rtpHeader.marker = marker;
    rtpPacket->rtpHeader.seq = seq;
    rtpPacket->rtpHeader.timestamp = timestamp;
    rtpPacket->rtpHeader.ssrc = ssrc;
}

int rtpSendPacket(int socket, uint8_t rtpChannel, struct RtpPacket* rtpPacket, uint32_t dataSize)
{
            
   
     
     
    int ret;

    rtpPacket->header[0] = '$';
    rtpPacket->header[1] = rtpChannel;
    rtpPacket->header[2] = ((dataSize+RTP_HEADER_SIZE) & 0xFF00 ) >> 8;
    rtpPacket->header[3] = (dataSize+RTP_HEADER_SIZE) & 0xFF;

    rtpPacket->rtpHeader.seq = htons(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = htonl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = htonl(rtpPacket->rtpHeader.ssrc);

    ret = send(socket, (void*)rtpPacket, dataSize+RTP_HEADER_SIZE+4, 0);

    rtpPacket->rtpHeader.seq = ntohs(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = ntohl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = ntohl(rtpPacket->rtpHeader.ssrc);

    return ret;
}
```

### 四、测试 

将`rtsp_server.c`、`tcp_rtp.h`、`tcp_rtp.c`保存下来

编译运行，程序默认打开`test.h264`，如果你没有视频源的话，可以从[RtspServer][]的example目录下获取

```java
# gcc rtsp_server.c tcp_rtp.c
# ./a.out
```

运行后得到一个url

```java
rtsp://127.0.0.1:8554
```

如何启动RTP OVER TCP?

需要设置vlc的模式

打开`工具`>>`首选项`>>`输入/编解码器`>>`live555 流传输`>>`RTP over RTSP(TCP)`

然后选择RTP over RTSP(TCP)

点击保存

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c555df7a34d359d7a7dac88a77cbe5bb.png)

输入url

 *  运行效果
    
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5e54683087b799c3d80d6e87fc628a43.png)

可算把这个系列写完了，还真是累，当然希望对于学习RTSP协议的人有所帮助，这也是我的初衷


[-RtspServer]: https://blog.csdn.net/weixin_42462202/article/details/98956346
[RTSP_RTSP]: https://blog.csdn.net/weixin_42462202/article/details/98986535
[RTSP_RTSP 1]: https://blog.csdn.net/weixin_42462202/article/details/99068041
[RTSP_RTP_H.264]: https://blog.csdn.net/weixin_42462202/article/details/99089020
[RTSP_H.264_RTSP]: https://blog.csdn.net/weixin_42462202/article/details/99111635
[RTSP_RTP_AAC]: https://blog.csdn.net/weixin_42462202/article/details/99200935
[RTSP_AAC_RTSP]: https://blog.csdn.net/weixin_42462202/article/details/99311193
[RTSP_RTP]: https://blog.csdn.net/weixin_42462202/article/details/99329048
[RTSP_RTSP 2]: https://blog.csdn.net/weixin_42462202/article/details/99413781
[RTSP_RTP OVER RTSP_TCP_RTSP]: https://blog.csdn.net/weixin_42462202/article/details/99442338
[RTSP_RTP OVER RTSP_TCP_RTSP 1]: #RTSPRTP_OVER_RTSPTCPRTSP_23
[RTP OVER RTSP_TCP]: #RTP_OVER_RTSPTCP_26
[1.1 _RTP]: #11_RTP_28
[1.2 _RTSP_RTP_RTCP]: #12_RTSPRTPRTCP_34
[RTP OVER RTSP_TCP_RTSP]: #RTP_OVER_RTSPTCPRTSP_58
[OPTIONS]: #OPTIONS_60
[DESCRIBE]: #DESCRIBE_79
[SETUP]: #SETUP_108
[PLAY]: #PLAY_131
[Link 1]: #_153
[3.1 RTP]: #31_RTP_155
[3.2]: #32__206
[3.2.1 _socket]: #321_socket_210
[3.2.2]: #322__231
[3.2.3]: #323__247
[3.2.4]: #324__290
[3.2.5 _RTP]: #325_RTP_361
[3.3]: #33__397
[rtsp_server.c]: #rtsp_serverc_399
[tcp_rtp.h]: #tcp_rtph_929
[tcp_rtp.c]: #tcp_rtpc_1002
[Link 2]: #_1056
[RtspServer]: https://github.com/ImSjt/RtspServer