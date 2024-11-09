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

## 从零开始写一个RTSP服务器（二）RTSP协议的实现 

#### 文章目录 

 *  [从零开始写一个RTSP服务器（二）RTSP协议的实现][RTSP_RTSP 3]
 *   *  [写在前面][Link 1]
     *  [一、创建套接字][Link 2]
     *  [二、解析请求][Link 3]
     *  [三、OPTIONS响应][OPTIONS]
     *  [四、DESCRIBE响应][DESCRIBE]
     *  [五、SETUP响应][SETUP]
     *  [六、PLAY响应][PLAY]
     *  [七、源码][Link 4]
     *  [八、测试][Link 5]

### 写在前面 

此系列只追求精简，旨在学习RTSP协议的实现过程，不追求复杂完美，所以这里要实现的RTSP服务器为了简单，实现上同一时间只能有一个客户端，下面开始介绍实现过程

在写一个RTSP服务器之前，我们必须知道一个RTSP服务器最简单的包含两部分，一部分是RTSP的交互，一部分是RTP发送，本文先实现RTSP交互过程

### 一、创建套接字 

想一下我们在vlc输入`rtsp://127.0.0.1:8554`后发生了什么事？

在这种情况下，vlc其实是一个rtsp客户端，当输入这个url后，vlc知道目的IP为`127.0.0.1`，目的端口号为`8854`，这时vlc会发起一个tcp连接取连接服务器，连接成功后就开始发送请求，服务端响应

所以我们要写一个rtsp服务器，第一步肯定是创建tcp服务器

首先创建tcp套接字，绑定端口，监听

 *  创建套接字
    
    ```java
    /*
     * 作者：_JT_
     * 博客：https://blog.csdn.net/weixin_42462202
     */
    serverSockfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(serverSockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    ```
 *  绑定地址和端口号
    
    ```java
    bind(serverSockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)
    ```
    
    这个示例绑定的地址是`INADDR_ANY`，端口号为`8554`
 *  开始监听
    
    ```java
    listen(serverSockfd, 10);
    ```

RTSP服务器传输音视频数据和信息使用的是RTP和RTCP，所以我们还要为RTP和RTCP创建UDP套接字，并绑定号端口

 *  创建套接字
    
    ```java
    serverRtpSockfd = createUdpSocket();
    serverRtcpSockfd = createUdpSocket();
    ```
 *  绑定端口号
    
    ```java
    bindSocketAddr(serverRtpSockfd, "0.0.0.0", SERVER_RTP_PORT);
    bindSocketAddr(serverRtcpSockfd, "0.0.0.0", SERVER_RTCP_PORT);
    ```

当创建好套接字还有绑定号端口后，就可以接收客户端请求了

 *  开始accept等待客户端连接
    
    ```java
    clientfd = accept(serverSockfd, (struct sockaddr *)&addr, &len);
    ```

### 二、解析请求 

当rtsp客户端连接成功后就会开始发送请求，服务器这是需要接收客户端请求并开始解析，再采取相应得操作

请求的格式为（详细参考上一篇[从零开始写一个RTSP服务器（一）不一样的RTSP协议讲解][RTSP_RTSP]）

```java
OPTIONS rtsp://127.0.0.1:8554/live RTSP/1.0\r\n
CSeq: 2\r\n
\r\n
```

```java
DESCRIBE rtsp://127.0.0.1:8554/live RTSP/1.0\r\n
CSeq: 3\r\n
Accept: application/sdp\r\n
\r\n
```

```java
SETUP rtsp://127.0.0.1:8554/live/track0 RTSP/1.0\r\n
CSeq: 4\r\n
Transport: RTP/AVP;unicast;client_port=54492-54493\r\n
\r\n
```

```java
PLAY rtsp://127.0.0.1:8554/live RTSP/1.0\r\n
CSeq: 5\r\n
Session: 66334873\r\n
Range: npt=0.000-\r\n
\r\n
```

这里我们做得最简单，首先解析`第一行`得到`方法`，对于`OPTIONS`、`DESCRIBE`、`PLAY`、`TEARDOWN`我们只解析`CSeq`。对于`SETUP`，我们讲`client_port`解析出来

所以我们要做的第一步就是解析请求中的信息

 *  接收客户端数据
    
    ```java
    recvLen = recv(clientSockfd, rBuf, BUF_MAX_SIZE, 0);
    ```
    
    这里实现了一个简单得函数`getLineFromBuf`，从buf中读取一行(\\r\\n)
 *  解析第一行请求得到方法
    
    ```java
    sscanf(line, "%s %s %s\r\n", method, url, version);
    ```
 *  其次解析`CSeq`
    
    ```java
    sscanf(line, "CSeq: %d\r\n", &cseq)
    ```
 *  如果方法是`SETUP`则再解析`client_port`
    
    ```java
    if(!strcmp(method, "SETUP"))
    {
    	sscanf(line, "Transport: RTP/AVP;unicast;client_port=%d-%d\r\n",
    		&clientRtpPort, &clientRtcpPort);
    }
    ```

解析完请求命令后，接下来就是更具不同得方法做不同的响应了，如下

```java
/*
 * 作者：_JT_
 * 博客：https://blog.csdn.net/weixin_42462202
 */
if(!strcmp(method, "OPTIONS"))
{
            
   
     
     
	handleCmd_OPTIONS();
}
else if(!strcmp(method, "DESCRIBE"))
{
            
   
     
     
	handleCmd_DESCRIBE();
}
else if(!strcmp(method, "SETUP"))
{
            
   
     
     
	handleCmd_SETUP();
}
else if(!strcmp(method, "PLAY"))
{
            
   
     
     
	handleCmd_PLAY();
}
else if(!strcmp(method, "TEARDOWN"))
{
            
   
     
     
	handleCmd_TEARDOWN();
}
```

### 三、OPTIONS响应 

OPTIONS是客户端向服务端请求可用的方法，我们这里就向客户端回复我们当前可用的方法

```java
sprintf(sBuf, "RTSP/1.0 200 OK\r\n"
				"CSeq: %d\r\n"
				"Public: OPTIONS, DESCRIBE, SETUP, PLAY\r\n"
				"\r\n",
				cseq);
send(clientSockfd, sBuf, strlen(sBuf));
```

### 四、DESCRIBE响应 

DESCRIBE是客户端向服务器请求媒体信息，这是服务器需要回复sdp描述文件，这个例子中的媒体是H.264

 *  sdp文件生成
    
    ```java
    sprintf(sdp, "v=0\r\n"
    			"o=- 9%ld 1 IN IP4 %s\r\n"
    			"t=0 0\r\n"
    			"a=control:*\r\n"
    			"m=video 0 RTP/AVP 96\r\n"
    			"a=rtpmap:96 H264/90000\r\n"
    			"a=control:track0\r\n",
    			time(NULL), localIp);
    ```
 *  回复
    
    ```java
    sprintf(sBuf, "RTSP/1.0 200 OK\r\n"
    		"CSeq: %d\r\n"
    		"Content-Base: %s\r\n"
    		"Content-type: application/sdp\r\n"
    		"Content-length: %d\r\n\r\n"
    		"%s",
    		cseq,
    		url,
    		strlen(sdp),
    		sdp);
    		
    send(clientSockfd, sBuf, strlen(sBuf));
    ```

### 五、SETUP响应 

SETUP是客户端请求建立会话连接，并发送了客户端的RTP端口和RTCP端口，那么此时服务端需要回复服务端的RTP端口和RTCP端口

 *  回复
    
    ```java
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
    
    send(clientSockfd, sBuf, strlen(sBuf));
    ```
    
    其中session id是随便写的，只要保证在多个会话连接时唯一的就行
    
    play响应之后就可以向客户端的RTP端口发送RTP包了

### 六、PLAY响应 

PLAY时客户端向服务器请求播放，这时服务端回复完请求后就开始通过setup过程中创建的udp套接字发送RTP包

 *  回复
    
    ```java
    sprintf(result, "RTSP/1.0 200 OK\r\n"
    				"CSeq: %d\r\n"
    				"Range: npt=0.000-\r\n"
    				"Session: 66334873; timeout=60\r\n\r\n",
    				cseq);
    
    send(clientSockfd, sBuf, strlen(sBuf));
    ```
 *  开始发送数据
    
    回复之后，就开始向客户端指定的RTP端口发送RTP包，如何发送RTP包，下篇文章再介绍

### 七、源码 

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

#define SERVER_PORT     8554
#define SERVER_RTP_PORT  55532
#define SERVER_RTCP_PORT 55533
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
    }
out:
    close(clientSockfd);
    free(rBuf);
    free(sBuf);
}

int main(int argc, char* argv[])
{
            
   
     
     
    int serverSockfd;
    int serverRtpSockfd, serverRtcpSockfd;
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

### 八、测试 

编译运行源码，打开vlc，输入`rtsp://127.0.0.1:8554`，点击开始播放，可以看到控制台会打印出交互过程，或是用wireshak抓包

本篇文章到这里结束，至此完成了RTSP协议的交互部分，在PLAY之后并没有开始发送RTP包，所以暂时还看不到视频，究竟如何发送RTP包，请看下一篇文章


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
[RTSP_RTSP 3]: #RTSPRTSP_23
[Link 1]: #_26
[Link 2]: #_32
[Link 3]: #_91
[OPTIONS]: #OPTIONS_188
[DESCRIBE]: #DESCRIBE_201
[SETUP]: #SETUP_235
[PLAY]: #PLAY_260
[Link 4]: #_280
[Link 5]: #_618