//http请求的读取和分析
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>

//主状态机可能的状态
enum CHECK_STATE
{
    CHECK_STATE_REQUESTION = 0,//正在分析当前请求行
    CHECK_STATE_HEADER//正在分析头部字段
};

//从状态机可能的状态
enum LINE_STATUS
{
    LINE_OK = 0,//读取到一个完整的行
    LINE_BAD,//行出错
    LINE_OPEN,//行数据尚且不完整
};

//服务器处理http请求的结果
enum HTTP_CODE
{
    NO_REQUEST,//请求不完整需要继续读取
    GET_REQUEST,//得到了一个完整的请求
    BAD_REQUEST,//请求有语法错误
    FORBIDDEN_REQUEST,//没有足够的权限
    INTERNAL_ERROR,//服务器内部错误
    CLOSED_CONNECTION//客户端连接已关闭
};

static const  char* szret[] = 
{
    "i get a corret result\n",
     "Sonmething wrong\n"
};



//从状态机，用于解析出一行内容
LINE_STATES pares_line(char * buffer, int & checked_index/*指向缓冲区正在分析的数据*/, int & read_index/*指向缓冲区尾部的数据，理解为buffuer尾*/)
{
    char temp;
    for( ; checked_index < read_index; ++checked_index)
    {
        //获得当前分析的字
        temp = buffer[checked_index];
        //如果当前的字节是\r，即回车符，则说明可能读取到一个完整的行
        if(temp == '\r')
        {
            //如果\r字符碰巧是目前buffer中的最后一个已经被读入的客户数据，那么这次分析没有读取到一个完整的行，需要继续读取数据
            if(check_index + 1 == read_index)
                return LINE_OPEN;
            
            //表示读到了一个完整的行
            if(temp == '\n')
            {
                buffer[check_index++] == '\0';
                buffer[check_index++] == '\0';
                return LINE_OK;
            }
            
            //以上都不是，就是存在语法错误
            return LINE_BAD;

        }
        //当前字符为\n也有可能是到了一行的情况
        else if(temp == '\n')
        {
            //因为\r\n一起用，还得判断
            if(check_index > 1 && buffer[check_index - 1] == '\r')
            {
                buffer[check_index--] == '\0';
                buffer[check_index++] == '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        
    }
    //如果所有的字符都读完了还没有遇到
    return LINE_OPEN;

}

//分析请求行
HTTP_CODE parse_requestline(char * temp, CHECK_STATE & checkstate)
{
    /*
    temp
    |
    GET /chapter/user.html HTTP/1.1\0  (尾部的\0是从状态机加的)
    */
    /*
      url
       |
    GET /chapter/user.html HTTP/1.1\0  
    */
    char* url = strpbrk(temp," \t");//strpbrk函数比较两个字符串中是否有相同的字符，有的话返回第一个相同字符的指针
    if(!url)//没有空格或者\t肯定有问题的不
    {
        return BAD_REQUEST;
    }
    *url++ = '\0';
    /*
        url
         |
    GET\0/chapter/user.html HTTP/1.1\0  
    */
    /*
    mth url
    |    |
    GET\0/chapter/user.html HTTP/1.1\0  
    */
    char * method = temp;
    if(strcasecmp(method, "GET") == 0)//仅支持GET方法,strcasecmp函数不计大小写判等
    {
        printf("The request method is GET\n");
    }  
    else
    {
        return BAD_REQUEST;
    }

    //直接跳到HTTP版本
    /*
        url
         |
    GET\0/chapter/user.html HTTP/1.1\0  
    */
    url += strspn(url," \t");//该函数返回 str1 中第一个不在字符串 str2 中出现的字符下标
    char * version = strpbrk(url," \t");
    /*
        url             version
         |                 |
    GET\0/chapter/user.html HTTP/1.1\0  
    */
    if(!version)
    {
        return BAD_REQUEST;
    }
    *version++ = '\0';
    //这样就把请求行解析成了三块，每一块用\0分割
    /*
    mth url               version
    |    |                   |
    GET\0/chapter/user.html\0HTTP/1.1\0
    */
    version += strspn(version," \t");//防止\0后面还有空格或者制表符，清理一下
    if(strcasecmp(version,"http/1.1") != 0)
    {
        return BAD_REQUEST;
    }
    //检查url是否合法
    if(strcasecmp(url,"https//:",7) == 0)
    {
        url += 7;
        url = strchr(url,'/');//在参数url所指向的字符串中搜索第一次出现字符'/'（一个无符号字符）的位置

    }
    if(!url || url[0] != '/')
    {
        return BAD_REQUEST;
    }

    pritnf("The requset URL is: %s\n",url);
    //http请求行处理完毕，转台转移到头部字段的分析
    check_state = CHECK_STATE_HEAD;
    
}