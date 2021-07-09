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

