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

## 从零开始写一个RTSP服务器（一）不一样的RTSP协议讲解 

#### 文章目录 

 *  [从零开始写一个RTSP服务器（一）不一样的RTSP协议讲解][RTSP_RTSP 3]
 *   *  [前言][Link 1]
     *  [一、什么是RTSP协议？][RTSP]
     *  [二、RTSP协议详解][RTSP 1]
     *   *  [2.1 RTSP数据格式][2.1 RTSP]
         *  [2.2 RTSP请求的常用方法][2.2 RTSP]
         *  [2.3 RTSP交互过程][2.3 RTSP]
         *  [2.4 sdp格式][2.4 sdp]
     *  [三、RTP协议][RTP]
     *   *  [3.1 RTP包格式][3.1 RTP]
         *  [3.2 RTP OVER TCP][]
     *  [四、RTCP][RTCP]

### 前言 

 *  为什么要写这个系列？
    
    因为我自己在学习rtsp协议想自己从零写一个rtsp服务器的时候，由于rtsp比较复杂，所以觉得这个过程非常的困难，网上许多相关文章或模棱两可，或是复制粘贴。所以想写这样一个系列，来帮助想要学习rtsp协议或者想要从零写一个rtsp服务器的初学者
 *  本系列的文章特点
    
    并系列文章实现追求精简，能够让人明白rtsp协议的实现过程，不追求复杂和完美
    
    如果想要实现一个比较完善的rtsp服务器，可以参考[我的开源项目-RtspServer][-RtspServer]

言归正传，下面开始本系列的文章

### 一、什么是RTSP协议？ 

RTSP是一个实时传输流协议，是一个应用层的协议

通常说的RTSP包括`RTSP协议`、`RTP协议`、`RTCP协议`

对于这些协议的作用简单的理解如下

RTSP协议：负责服务器与客户端之间的请求与响应

RTP协议：负责传输媒体数据

RTCP协议：在RTP传输过程中提供传输信息

rtsp承载与rtp和rtcp之上，rtsp并不会发送媒体数据，而是使用rtp协议传输

rtp并没有规定发送方式，可以选择udp发送或者tcp发送

### 二、RTSP协议详解 

rtsp的交互过程就是客户端请求，服务器响应，下面看一看请求和响应的数据格式

#### 2.1 RTSP数据格式 

RTSP协议格式与HTTP协议格式类似

 *  RTSP客户端的请求格式
    
    ```java
    method url vesion\r\n
    CSeq: x\r\n
    xxx\r\n
    ...
    \r\n
    ```
    
    method：方法，表明这次请求的方法，rtsp定义了很多方法，稍后介绍
    
    url：格式一般为`rtsp://ip:port/session`，ip表主机ip，port表端口好，如果不写那么就是默认端口，rtsp的默认端口为`554`，session表明请求哪一个会话
    
    version：表示rtsp的版本，现在为`RTSP/1.0`
    
    CSeq：序列号，每个RTSP请求和响应都对应一个序列号，序列号是递增的
 *  RTSP服务端的响应格式
    
    ```java
    vesion 200 OK\r\n
    CSeq: x\r\n
    xxx\r\n
    ...
    \r\n
    ```
    
    version：表示rtsp的版本，现在为`RTSP/1.0`
    
    CSeq：序列号，这个必须与对应请求的序列号相同

#### 2.2 RTSP请求的常用方法 

<table> 
 <thead> 
  <tr> 
   <th>方法</th> 
   <th>描述</th> 
  </tr> 
 </thead> 
 <tbody> 
  <tr> 
   <td>OPTIONS</td> 
   <td>获取服务端提供的可用方法</td> 
  </tr> 
  <tr> 
   <td>DESCRIBE</td> 
   <td>向服务端获取对应会话的媒体描述信息</td> 
  </tr> 
  <tr> 
   <td>SETUP</td> 
   <td>向服务端发起建立请求，建立连接会话</td> 
  </tr> 
  <tr> 
   <td>PLAY</td> 
   <td>向服务端发起播放请求</td> 
  </tr> 
  <tr> 
   <td>TEARDOWN</td> 
   <td>向服务端发起关闭连接会话请求</td> 
  </tr> 
 </tbody> 
</table>

#### 2.3 RTSP交互过程 

有了上述的知识，我们下面来讲解一个RTSP的交互过程

OPTIONS

 *  C–>S
    
    ```java
    OPTIONS rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 2\r\n
    \r\n
    ```
    
    客户端向服务器请求可用方法
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 2\r\n
    Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY\r\n
    \r\n
    ```
    
    服务端回复客户端，当前可用方法OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY

DESCRIBE

 *  C–>S
    
    ```java
    DESCRIBE rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 3\r\n
    Accept: application/sdp\r\n
    \r\n
    ```
    
    客户端向服务器请求媒体描述文件，格式为sdp
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 3\r\n
    Content-length: 146\r\n
    Content-type: application/sdp\r\n
    \r\n
    
    v=0\r\n
    o=- 91565340853 1 in IP4 192.168.31.115\r\n
    t=0 0\r\n
    a=contol:*\r\n
    m=video 0 RTP/AVP 96\r\n
    a=rtpmap:96 H264/90000\r\n
    a=framerate:25\r\n
    a=control:track0\r\n
    ```
    
    服务器回复了sdp文件，这个文件告诉客户端当前服务器有哪些音视频流，有什么属性，具体稍后再讲解
    
    这里只需要直到客户端可以根据这些信息得知有哪些音视频流可以发送

SETUP

 *  C–>S
    
    ```java
    SETUP rtsp://192.168.31.115:8554/live/track0 RTSP/1.0\r\n
    CSeq: 4\r\n
    Transport: RTP/AVP;unicast;client_port=54492-54493\r\n
    \r\n
    ```
    
    客户端发送建立请求，请求建立连接会话，准备接收音视频数据
    
    解析一下`Transport: RTP/AVP;unicast;client_port=54492-54493\r\n`
    
    RTP/AVP：表示RTP通过UDP发送，如果是RTP/AVP/TCP则表示RTP通过TCP发送
    
    unicast：表示单播，如果是multicast则表示多播
    
    client\_port=54492-54493：由于这里希望采用的是RTP OVER UDP，所以客户端发送了两个用于传输数据的端口，客户端已经将这两个端口绑定到两个udp套接字上，`54492`表示是RTP端口，`54493`表示RTCP端口(RTP端口为某个偶数，RTCP端口为RTP端口+1)
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 4\r\n
    Transport: RTP/AVP;unicast;client_port=54492-54493;server_port=56400-56401\r\n
    Session: 66334873\r\n
    \r\n
    ```
    
    服务端接收到请求之后，得知客户端要求采用`RTP OVER UDP`发送数据，`单播`，`客户端`用于传输`RTP`数据的端口为`54492`，RTCP的端口为`54493`
    
    服务器也有两个`udp套接字`，绑定好两个端口，一个用于传输RTP，一个用于传输RTCP，这里的端口号为`56400-56401`
    
    之后客户端会使用`54492-54493`这两端口和服务器通过udp传输数据，服务器会使用`56400-56401`这两端口和这个客户端传输数据

PLAY

 *  C–>S
    
    ```java
    PLAY rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 5\r\n
    Session: 66334873\r\n
    Range: npt=0.000-\r\n
    \r\n
    ```
    
    客户端请求播放媒体
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 5\r\n
    Range: npt=0.000-\r\n
    Session: 66334873; timeout=60\r\n
    \r\n
    ```
    
    服务器回复之后，会开始使用RTP通过udp向客户端的54492端口发送数据

TEARDOWN

 *  C–>S
    
    ```java
    TEARDOWN rtsp://192.168.31.115:8554/live RTSP/1.0\r\n
    CSeq: 6\r\n
    Session: 66334873\r\n
    \r\n
    ```
 *  S–>C
    
    ```java
    RTSP/1.0 200 OK\r\n
    CSeq: 6\r\n
    \r\n
    ```

#### 2.4 sdp格式 

我们上面避开没有讲sdp文件，这里来好好补一补

sdp格式由多行的`type=value`组成

`sdp会话描述`由一个`会话级描述`和多个`媒体级描述`组成。会话级描述的作用域是整个会话，媒体级描述描述的是一个视频流或者音频流

`会话级描述`由`v=`开始到第一个媒体级描述结束

`媒体级描述`由`m=`开始到下一个媒体级描述结束

下面是上面示例的sdp文件，我们就来好好分析一下这个sdp文件

```java
v=0\r\n
o=- 91565340853 1 in IP4 192.168.31.115\r\n
t=0 0\r\n
a=contol:*\r\n
m=video 0 RTP/AVP 96\r\n
a=rtpmap:96 H264/90000\r\n
a=framerate:25\r\n
a=control:track0\r\n
```

这个示例的sdp文件包含`一个会话级描述`和`一个媒体级描述`，分别如下

 *  会话级描述
    
    ```java
    v=0\r\n
    o=- 91565340853 1 IN IP4 192.168.31.115\r\n
    t=0 0\r\n
    a=contol:*\r\n
    ```
    
    v=0
    
    表示sdp的版本  
    o=- 91565340853 1 IN IP4 192.168.31.115  
    格式为 o=<用户名> <会话id> <会话版本> <网络类型><地址类型> <地址>  
    用户名：-  
    会话id：91565340853，表示rtsp://192.168.31.115:8554/live请求中的live这个会话  
    会话版本：1  
    网络类型：IN，表示internet  
    地址类型：IP4，表示ipv4  
    地址：192.168.31.115，表示服务器的地址
 *  媒体级描述
    
    ```java
    m=video 0 RTP/AVP 96\r\n
    a=rtpmap:96 H264/90000\r\n
    a=framerate:25\r\n
    a=control:track0\r\n
    ```
    
    m=video 0 RTP/AVP 96\\r\\n
    
    格式为 m=<媒体类型> <端口号> <传输协议> <媒体格式 >  
    媒体类型：video
    
    端口号：0，为什么是0？因为上面在`SETUP`过程会告知端口号，所以这里就不需要了
    
    传输协议：RTP/AVP，表示RTP OVER UDP，如果是RTP/AVP/TCP，表示RTP OVER TCP
    
    媒体格式：表示负载类型(payload type)，一般使用96表示H.264
    
    a=rtpmap:96 H264/90000
    
    格式为a=rtpmap:<媒体格式><编码格式>/<时钟频率>
    
    a=framerate:25
    
    表示帧率
    
    a=control:track0
    
    表示这路视频流在这个会话中的编号

### 三、RTP协议 

#### 3.1 RTP包格式 

rtp包由rtp头部和rtp荷载构成

 *  RTP头部

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f711a8e3015bd9149c6b99c5aca4e39a.png)

版本号(V)：2Bit，用来标志使用RTP版本

填充位§：1Bit，如果该位置位，则该RTP包的尾部就包含填充的附加字节

扩展位(X)：1Bit，如果该位置位，则该RTP包的固定头部后面就跟着一个扩展头部

CSRC技术器(CC)：4Bit，含有固定头部后面跟着的CSRC的数据

标记位(M)：1Bit，该位的解释由配置文档来承担

载荷类型(PT)：7Bit，标识了RTP载荷的类型

序列号(SN)：16Bit，发送方在每发送完一个RTP包后就将该域的值增加1，可以由该域检测包的丢失及恢复

包的序列。序列号的初始值是随机的

时间戳：32比特，记录了该包中数据的第一个字节的采样时刻

同步源标识符(SSRC)：32比特，同步源就是RTP包源的来源。在同一个RTP会话中不能有两个相同的SSRC值

贡献源列表(CSRC List)：0-15项，每项32比特，这个不常用

 *  rtp荷载
    
    rtp载荷为音频或者视频数据

#### 3.2 RTP OVER TCP 

RTP默认是采用UDP发送的，格式为RTP头+RTP载荷，如果是使用TCP，那么需要在RTP头之前再加上四个字节

第一个字节：$，辨识符

第二个字节：通道，在SETUP的过程中获取

第三第四个字节： RTP包的大小，最多只能12位，第三个字节保存高4位，第四个字节保存低8位

### 四、RTCP 

RTCP用于在RTP传输过程中提供传输信息，可以报道RTP传输情况，还可以用来音视频同步，这里就不详细讲解了


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
[RTSP]: #RTSP_40
[RTSP 1]: #RTSP_58
[2.1 RTSP]: #21_RTSP_62
[2.2 RTSP]: #22_RTSP_98
[2.3 RTSP]: #23_RTSP_108
[2.4 sdp]: #24_sdp_252
[RTP]: #RTP_330
[3.1 RTP]: #31_RTP_332
[3.2 RTP OVER TCP]: #32_RTP_OVER_TCP_366
[RTCP]: #RTCP_376