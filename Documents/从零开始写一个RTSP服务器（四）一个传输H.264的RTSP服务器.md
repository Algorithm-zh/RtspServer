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

## 从零开始写一个RTSP服务器（四）一个传输H.264的RTSP服务器 

#### 文章目录 

 *  [从零开始写一个RTSP服务器（四）一个传输H.264的RTSP服务器][RTSP_H.264_RTSP 1]
 *   *  [一、建立套接字][Link 1]
     *  [二、接收客户端连接][Link 2]
     *  [三、解析命令][Link 3]
     *  [四、处理请求][Link 4]
     *   *  [4.1 OPTIONS][]
         *  [4.2 DESCRIBE][]
         *  [4.3 SETUP][]
         *  [4.4 PLAY][]
     *  [五、H.264 RTP打包发送][H.264 RTP]
     *  [六、源码][Link 5]
     *   *  [h264\_rtsp\_server.c][h264_rtsp_server.c]
         *  [rtp.c][]
         *  [rtp.h][]
     *  [七、测试][Link 6]

  
本篇文章的目的：这篇文章的目的是实现一个播放H.264的RTSP服务器，相信如果是仔细看了前面几篇文章的话，这简直是易如反掌，就是把前面的东西拼到一起

所以这篇文章并不会讲述新的知识，只是把前面的东西拼凑到一起，整理一下思路，最后给出一个示例，下面开始讲解一下我提供的这个示例的运行流程

### 一、建立套接字 

一开始进入main函数后，就监听服务器tcp套接字，绑定端口号，然后开始监听

然后再分别建立用于RTP和RTCP的udp套接字，绑定好端口

然后进入循环中开始服务

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

main()
{
            
   
     
     
	/* 创建服务器tcp套接字，绑定端口，监听 */
    serverSockfd = createTcpSocket();
	bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT);
	listen(serverSockfd, 10);
	
    /* 建立用于RTP和RTCP的udp套接字，绑定好端口 */
    serverRtpSockfd = createUdpSocket();
    serverRtcpSockfd = createUdpSocket();
    bindSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT);
    bindSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT);
    
	while(1)
	{
            
   
     
     
        ...
	}
}
```

### 二、接收客户端连接 

在while循环中接收客户端，然后调用doClient服务

```java
main()
{
            
   
     
     
    ...
    while(1)
	{
            
   
     
     
        clientSockfd = acceptClient(serverSockfd, clientIp, &clientPort);
        doClient(clientSockfd, clientIp, clientPort, serverRtpSockfd, serverRtcpSockfd);
	}
}
```

上面其实就是一个TCP服务器的基本步骤，没有什么特别的

下面来看一看doClient函数

### 三、解析命令 

doClient就是一个while循环（这是一个同时只能服务一个客户的服务器），不断地接收命令解析命令，然后调用相应地操作

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */
 
doClient()
{
            
   
     
     
	while(1)
	{
            
   
     
     
     	recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);   
        ...
        sscanf(line, "%s %s %s\r\n", method, url, version);
        ...
        sscanf(line, "CSeq: %d\r\n", &cseq)
        ...
	}
}
```

### 四、处理请求 

在解析完客户端命令后，会调用相应的请求，处理完之后讲接收打印到`sBuf`中，然后发送给客户端

```java
doClient()
{
            
   
     
     
	while(1)
	{
            
   
     
     
		...
         /* 处理请求 */
         if(!strcmp(method, "OPTIONS"))
             handleCmd_OPTIONS(sBuf, cseq);
        else if(!strcmp(method, "DESCRIBE"))
            handleCmd_DESCRIBE(sBuf, cseq, url);
        else if(!strcmp(method, "SETUP"))
            handleCmd_SETUP(sBuf, cseq, clientRtpPort);
        else if(!strcmp(method, "PLAY"))
            handleCmd_PLAY(sBuf, cseq);
        
        /* 放回结果 */
        send(clientSockfd, sBuf, strlen(sBuf), 0);
	}
}
```

下面来看看各个请求的行动

#### 4.1 OPTIONS 

返回可用方法

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */
 
static int handleCmd_OPTIONS(char* result, int cseq)
{
            
   
     
     
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
                    "\r\n",
                    cseq);
                
    return 0;
}
```

#### 4.2 DESCRIBE 

返回sdp文件信息，这是一个H.264的媒体描述信息，详细内容已经在前面文章讲解过，这里不再累赘

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

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
```

#### 4.3 SETUP 

SETUP过程发送服务端RTP端口和RTCP端口

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort,
                            int* localRtpSockfd, int* localRtcpSockfd)
{
            
   
     
     
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
                    "Session: 66334873\r\n"
                    "\r\n",
                    cseq,
                    clientRtpPort,
                    clientRtpPort+1,
                    SERVER_RTP_PORT,
                    SERVER_RTCP_PORT);
    
    return 0;
}
```

#### 4.4 PLAY 

PLAY操作回复后，会开始发送RTP包

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

static int handleCmd_PLAY(char* result, int cseq)
{
            
   
     
     
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Range: npt=0.000-\r\n"
                    "Session: 66334873; timeout=60\r\n\r\n",
                    cseq);
    
    return 0;
}
```

### 五、H.264 RTP打包发送 

从H.264文件中读取一个NALU，向客户端发送RTP包（目的IP，目的RTP端口）

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

doClient()
{
            
   
     
     
	while(1)
	{
            
   
     
     
		...
		...
		...
		if(!strcmp(method, "PLAY"))
		{
            
   
     
     
            while(1)
            {
            
   
     
     
                /* 获取一帧 */
            	frameSize = getFrameFromH264File(fd, frame, 500000);
                
                /* RTP打包发送 */
            	rtpSendH264Frame(localRtpSockfd, clientIP, clientRtpPort,
            					rtpPacket, frame+startCode, frameSize);
		
            }
	
        }

    }
}
```

下面看一看RTP打包过程，RTP打包实现了单NALU打包和分片打包

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */

static int rtpSendH264Frame(int socket, const char* ip, int16_t port,
                            struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize)
{
            
   
     
     
    /* 如果包比较小，则采用单NALU打包 */
    if (frameSize <= RTP_MAX_PKT_SIZE)
    {
            
   
     
     
        rtpSendPacket(socket, ip, port, rtpPacket, frameSize);
    }
    else //否则采用分片打包
    {
            
   
     
        
        for (i = 0; i < pktNum; i++)
        {
            
   
     
     
            /* 填充载荷的前两个字节 */
            rtpPacket->payload[0] = (naluType & 0x60) | 28;
            rtpPacket->payload[1] = naluType & 0x1F;
            
            /* 发送RTP包 */
            rtpSendPacket(socket, ip, port, rtpPacket, RTP_MAX_PKT_SIZE+2);
        }
    }
}
```

### 六、源码 

我也懒得建个Git仓了，源码就直接贴到这里吧，虽然有点长，嘻嘻

总共有3个文件，`h264_rtsp_server.c`、`rtp.c`、`rtp.h`

#### h264\_rtsp\_server.c 

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

#include "rtp.h"

#define H264_FILE_NAME   "test.h264"
#define SERVER_PORT      8554
#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533
#define BUF_MAX_SIZE     (1024*1024)

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

static int rtpSendH264Frame(int socket, const char* ip, int16_t port,
                            struct RtpPacket* rtpPacket, uint8_t* frame, uint32_t frameSize)
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
        ret = rtpSendPacket(socket, ip, port, rtpPacket, frameSize);
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
            ret = rtpSendPacket(socket, ip, port, rtpPacket, RTP_MAX_PKT_SIZE+2);
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
            ret = rtpSendPacket(socket, ip, port, rtpPacket, remainPktSize+2);
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

static int handleCmd_SETUP(char* result, int cseq, int clientRtpPort)
{
            
   
     
     
    sprintf(result, "RTSP/1.0 200 OK\r\n"
                    "CSeq: %d\r\n"
                    "Transport: RTP/AVP;unicast;client_port=%d-%d;server_port=%d-%d\r\n"
                    "Session: 66334873\r\n"
                    "\r\n",
                    cseq,
                    clientRtpPort,
                    clientRtpPort+1,
                    SERVER_RTP_PORT,
                    SERVER_RTCP_PORT);
    
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

static void doClient(int clientSockfd, const char* clientIP, int clientPort,
                                        int serverRtpSockfd, int serverRtcpSockfd)
{
            
   
     
     
    char method[40];
    char url[100];
    char version[40];
    int cseq;
    int clientRtpPort, clientRtcpPort;
    char *bufPtr;
    char* rBuf = malloc(BUF_MAX_SIZE);
    char* sBuf = malloc(BUF_MAX_SIZE);
    char line[400];

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

        /* 如果是SETUP，那么就再解析client_port */
        if(!strcmp(method, "SETUP"))
        {
            
   
     
     
            while(1)
            {
            
   
     
     
                bufPtr = getLineFromBuf(bufPtr, line);
                if(!strncmp(line, "Transport:", strlen("Transport:")))
                {
            
   
     
     
                    sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
                                    &clientRtpPort, &clientRtcpPort);
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
            
   
     
     
            if(handleCmd_SETUP(sBuf, cseq, clientRtpPort))
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
            printf("client ip:%s\n", clientIP);
            printf("client port:%d\n", clientRtpPort);

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
                rtpSendH264Frame(serverRtpSockfd, clientIP, clientRtpPort,
                                    rtpPacket, frame+startCode, frameSize);
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
    int serverRtpSockfd, serverRtcpSockfd;

    serverSockfd = createTcpSocket();
    if(serverSockfd < 0)
    {
            
   
     
     
        printf("failed to create tcp socket\n");
        return -1;
    }

    if(bindSocketAddr(serverSockfd, "0.0.0.0", SERVER_PORT) < 0)
    {
            
   
     
     
        printf("failed to bind addr\n");
        return -1;
    }

    if(listen(serverSockfd, 10) < 0)
    {
            
   
     
     
        printf("failed to listen\n");
        return -1;
    }

    serverRtpSockfd = createUdpSocket();
    serverRtcpSockfd = createUdpSocket();
    if(serverRtpSockfd < 0 || serverRtcpSockfd < 0)
    {
            
   
     
     
        printf("failed to create udp socket\n");
        return -1;
    }

    if(bindSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT) < 0 ||
        bindSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT) < 0)
    {
            
   
     
     
        printf("failed to bind addr\n");
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

        doClient(clientSockfd, clientIp, clientPort, serverRtpSockfd, serverRtcpSockfd);
    }

    return 0;
}
```

#### rtp.c 

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

#include "rtp.h"

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

int rtpSendPacket(int socket, const char* ip, int16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize)
{
            
   
     
     
    struct sockaddr_in addr;
    int ret;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);

    rtpPacket->rtpHeader.seq = htons(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = htonl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = htonl(rtpPacket->rtpHeader.ssrc);

    ret = sendto(socket, (void*)rtpPacket, dataSize+RTP_HEADER_SIZE, 0,
                    (struct sockaddr*)&addr, sizeof(addr));

    rtpPacket->rtpHeader.seq = ntohs(rtpPacket->rtpHeader.seq);
    rtpPacket->rtpHeader.timestamp = ntohl(rtpPacket->rtpHeader.timestamp);
    rtpPacket->rtpHeader.ssrc = ntohl(rtpPacket->rtpHeader.ssrc);

    return ret;
}
```

#### rtp.h 

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
            
   
     
     
    struct RtpHeader rtpHeader;
    uint8_t payload[0];
};

void rtpHeaderInit(struct RtpPacket* rtpPacket, uint8_t csrcLen, uint8_t extension,
                    uint8_t padding, uint8_t version, uint8_t payloadType, uint8_t marker,
                   uint16_t seq, uint32_t timestamp, uint32_t ssrc);
int rtpSendPacket(int socket, const char* ip, int16_t port, struct RtpPacket* rtpPacket, uint32_t dataSize);

#endif //_RTP_H_
```

### 七、测试 

将三个文件保存下来，`h264_rtsp_server.c`、`rtp.c`、`rtp.h`

编译

```java
# gcc h264_rtsp_server.c rtp.c
```

运行，程序默认会打开`test.h264`的视频文件，如果你没有视频源的话，可以从[RtspServer][]的example目录下获取

```java
# ./a.out
```

运行后会打印一个url

```java
rtsp://127.0.0.1:8554
```

在vlc中输入url，即可看到视频

 *  运行效果
    
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4353ce1ff4ff1ca6e5d41d83c8639e9b.png)


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
[RTSP_H.264_RTSP 1]: #RTSPH264RTSP_23
[Link 1]: #_30
[Link 2]: #_64
[Link 3]: #_84
[Link 4]: #_108
[4.1 OPTIONS]: #41_OPTIONS_136
[4.2 DESCRIBE]: #42_DESCRIBE_158
[4.3 SETUP]: #43_SETUP_198
[4.4 PLAY]: #44_PLAY_226
[H.264 RTP]: #H264_RTP_248
[Link 5]: #_315
[h264_rtsp_server.c]: #h264_rtsp_serverc_321
[rtp.c]: #rtpc_870
[rtp.h]: #rtph_925
[Link 6]: #_997
[RtspServer]: https://github.com/ImSjt/RtspServer