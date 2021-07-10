#2021年07月09日更新：


有限状态机解析http请求
http_conn对象记录每一个链接的状态
epoll + 半同步半反应堆线程池并发处理连接

实现方法：

（先做两个网页放进去，doc_root目录改成资源文件所在目录）
make server
./server 8888（./server + 端口号）

浏览器端：
公网ip:端口号/文件名

问题记录：

第一个：网站的根目录，在代码中应该写绝对路径才行。

第二个：调试问题，由于没有日志系统，有很多输入错误但是没被编译器察觉的错误很容易发现不了。这个时候应该
使用GBD进行多线程调试，配合printf或者打日志的办法定位到问题点，进行修改！

第三个：这个代码中跑起来实测不能进行大文件的传输，源代码问题如下：
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


##2021年07月10日更新：

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
      
   


