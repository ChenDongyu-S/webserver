

这是基于《linux高性能服务器》一书开发的一个轻量级服务器。
===============
作者希望不断完善使其成为一个稳定的可以实现全部网站和服务器功能的一个轻量级服务器！
主要技术：

* 使用 线程池 + 非阻塞socket + epoll + （reactor&proactor）事件处理的并发模型
* 使用状态机解析HTTP请求报文，支持POST与GET请求解析，并能显示登录注册功能
* 绑定域名，通过端口号可以请求服务器图片和视频文件（文件大小超100m）
* 实现同步/异步（基于阻塞队列）日志系统，监控服务器状态
* 经Webbench压力测试可以实现上万的并发连接数据交换

项目网站地址：http://chendongyu.top:9000/log.html

搭建方法：
# 搭建数据库：（参考自www.runoob.com）
```C++
//mysql 安装
...
```
* 数据库连接：
```C++
mysql -u root -p
```
* 数据库用户设置
```C++
mysql> INSERT INTO user 
          (host, user, password, 
           select_priv, insert_priv, update_priv) 
           VALUES ('localhost', 'guest', 
           PASSWORD('guest123'), 'Y', 'Y', 'Y');

mysql> FLUSH PRIVILEGES;
``` 
* 创建数据库
```C++
mysql> create DATABASE RUNOOB;
```
* 创建表与添加数据
```C++
mysql>USE yourdb;
        CREATE TABLE user(
            username char(50) NULL,
            passwd char(50) NULL
        )ENGINE=InnoDB;

// 添加数据
    INSERT INTO user(username, passwd) VALUES('name', 'passwd');
```

* 修改main.cpp中你设置的数据库信息

# 服务器部署
* 下载源码
* 修改http_conn.cpp文件中的网页资源目录doc_root（修改为你的资源所在的绝对目录）
* make sever编译文件
* ./server + 端口号
* 浏览器端：
*   公网：公网ip:端口号/目标文件名（如果想去掉目标文件名，则在http_conn修改逻辑即可，设置一个默认主页面，如果想使用域名，申请绑定备案即可）
*   内网：内网ip:端口号/目标文件名（如果想挂在公网，请先确定有无公网IP，有的话，需要路由做端口映射，没有的话，可以使用内网穿透）
* 项目框架：http://chendongyu.top:9000/log.html
* 

# 服务器测试
* 下载webbench
```
[root@iZbp1iw4hobmafuvef0ijhZ webbench-1.5]# make webbench
[root@iZbp1iw4hobmafuvef0ijhZ webbench-1.5]# ./webbench -c 800 -t 20 http://chendongyu.top:9002/log.html
Webbench - Simple Web Benchmark 1.5
Copyright (c) Radim Kolar 1997-2004, GPL Open Source Software.

Benchmarking: GET http://chendongyu.top:9002/log.html
800 clients, running 20 sec.

Speed=51330 pages/min, 113768 bytes/sec.
Requests: 17110 susceed, 0 failed.

```
测试结果如下，测试平台配置：单核心CPU，1G内存，40gSSD
![image](https://user-images.githubusercontent.com/76243213/133824711-7794fe02-b939-4848-acca-cec5a4ecf0d7.png)



以下为更新日记

=====
-----------------------------------------------------------2021年09月15日更新-----------------------------------------------------------
新增了数据库连接池，实现了与用户交互，解析POST请求
遇到了不少问题，不过都是小问题，一一一解决：
1.从http请求中读不到正确的字段长度，原因：字段名打错了（少了一个：）！所以写程序时一定要认真细致，遵守代码规范，这种问题搞死人啊！
2.用RAII机制取出数据库连接时总是段错误，使用时要确认数据库连接池已经初始化！并且数据库初始化要先于线程池，后于http_conn
3.使用异步日志定位内存泄露时，由于日志是异步的，常常程序挂掉了，日志还没写好，难以定位，建议测试时可以使用同步日志。
4.使用时必须先行建立好数据库用户名和密码，必须先建立好表格，并且内置一个root用户，不然会查不到东西报错。


------------------------------------------------------------2021年07月09日更新------------------------------------------------------------：



第一个：网站的根目录，在代码中应该写绝对路径才行。

第二个：这个代码中跑起来实测不能进行大文件的传输，源代码问题如下：
问题在于bytes_to_send 和bytes_have_send记录文件传输进度的两个int变量的值未记录下需要传输文件的大小，
传输文件完毕的判定(bytes_to_send <= bytes_have_send)也很迷，这两点导致了在传输大文件的时候会出现问题，
目测只会传输报头大小的数据量？感觉是这样 没有具体测试。
将bytes_to_send 和bytes_have_send设置为类数据成员，初始化为0；
在process_write()判定需要读取文件时，设置为
bytes_to_send = m_write_idx + m_file_stat.st_size;//报头文件都要写
在其他时候，设置为
bytes_to_send = m_write_idx;//仅仅写报头
在write()中修改写入文件完毕的条件为：
if (bytes_to_send <= 0)
之后在根据文件读写操作进行代码的编写。


------------------------------------------------------------2021年07月10日更新------------------------------------------------------------：

加入了定时器：
  创建一个客户链接数组，里面包含了对应的文件描述符和计时器，计时器的任务就是等待15秒关闭链接。每次监听到链接时，创建一个对应文件描述符的计时器，将次计时器更新进入定时器链表，并将倒计时更新为15秒，设置每五秒触发一次ALARM信号。并绑定相应的应该处理函数，执行信号处理函数时，会遍历链表中的所有计时器，将到期的链接关闭。
  值得注意的是如果一个链接反复的发送请求，应该也要更新计时器的时间，不仅如此，最重要的是在读取的时候也要更新，考虑到一些大文件会一直持续读取长达几个小时，所以每次读取操作的时候也要更新计时器记录的时间。
  
解决了webbech测压失败的原因
   设置优雅关闭即可解决，之前设置的是1,0那么close直接立即返回，导致数据没有发送给客户端，导致了webbech测压失败。
   
   
     struct linger tmp = {1, 1};//1,0的设置会导致上述问题
     setsockopt(listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
     设置优雅断开
      #include <arpa/inet.h>
      struct linger {
        int l_onoff;
        int l_linger;
      };
      三种断开方式：

      1. l_onoff = 0; l_linger忽略
      close()立刻返回，底层会将未发送完的数据发送完成后再释放资源，即优雅退出。

      2. l_onoff != 0; l_linger = 0;
      close()立刻返回，但不会发送未发送完成的数据，而是通过一个REST包强制的关闭socket描述符，即强制退出。

      3. l_onoff != 0; l_linger > 0;
      close()不会立刻返回，内核会延迟一段时间，这个时间就由l_linger的值来决定。如果超时时间到达之前，发送
      完未发送的数据(包括FIN包)并得到另一端的确认，close()会返回正确，socket描述符优雅性退出。否则，close()
      会直接返回错误值，未发送数据丢失，socket描述符被强制性退出。需要注意的时，如果socket描述符被设置为非堵
      塞型，则close()会直接返回值。
      
 ------------------------------------------------------------2021年07月13日更新------------------------------------------------------------：


1.把主函数的内容抽象成为了一个server管理类，负责进行主流程的运行

2.发现一个bug，这个bug会导致再多次请求文件，并在文件传输完成前退出，之后，再次链接服务器，
首先会连接上，之后不进行读事件，而是直接触发EPOLLRDHUP信号，该浏览器的任何链接都会触发该
信号，而当我们换一个浏览器登录后，之前的浏览器再次连接时则不会产生EPOLLRDHUP信号。
经测试，如果每次访问中，文件都顺利传输完成，则不会出现上述情况，问题目前还在排查之中。


3.加入了日志系统，可以实现基于阻塞队列的异步日志，和直接写入的同步日志。日志采用单例模式实现，
每次通过一个静态函数调用该实例的指针在进行操作。
  具体实现，将必要的参数通过server类来传入和调用，记录下文件路径和文件名，并且使用fopen "a"的方式创造或者打开
  文件，通过尾部添加的方式写入日志。
  同步、异步具体实现：
    同步在调用的时候直接写入，
    异步在调用的时候不像同步那样直接写入，而是把需要写入的内容作为string传入到定义好了的阻塞队列中，异步
    视线中会独立开一个线程，执行将队列中的string不断写入日志文件的操作。  

 ------------------------------------------------------------2021年07月15日更新------------------------------------------------------------：
 
 做了一些东西一直没有上传，网络不稳定，一起说了：
 
 07月13日：
1.昨天的问题找了很久还没找到原因，暂时搁置。
2.加入了日志系统，可以实现基于阻塞队列的异步日志，和直接写入的同步日志。日志采用单例模式实现，
每次通过一个静态函数调用该实例的指针在进行操作。
  具体实现，将必要的参数通过server类来传入和调用，记录下文件路径和文件名，并且使用fopen "a"的方式创造或者打开
  文件，通过尾部添加的方式写入日志。
  同步、异步具体实现：
    同步在调用的时候直接写入，
    异步在调用的时候不像同步那样直接写入，而是把需要写入的内容作为string传入到定义好了的阻塞队列中，异步
    视线中会独立开一个线程，执行将队列中的string不断写入日志文件的操作。

07月14日：
1.昨天的问题终于找到了！
  原因是代码中使用了ET触发模式，然而新建连接accpt函数在处理信号时没有循环读取信号，导致水平信号留在文件描述符
  上，没有处理干净，虽然之后不触发，但是一直留着，等待下次边缘触发，结果就是没有收到想收到的时间，只收到了
  上次残留的事件。
  解决方案：
  1.accpt函数循环接受，处理干净
  2.改为默认LT触发模式

07月15日上午：
1.代码中设置了兼容ET和LT模式的代码，这个可以在main函数中修改。
2.新增reactor反应堆模式，也就是异步IO，主线程只负责监听，读写操作全部放入线程池中处理。这个也可以在main函数中修改。
  不过考虑到编译，以后更新，会把设置权交到用户手中，从命令行读取。
 
 
 
 
