#include <cstdio>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MSG_TYPE_LOGIN    0
#define MSG_TYPE_FILENAME 1
#define MSG_TYPE_DOWNLOAD 2
#define MSG_TYPE_UPLOAD 3
#define MSG_TYPE_UPLOAD_DATA 4

typedef struct msg
{
    int type; //协议类型 0表示登录协议包 1表示文件名传输包，2.文件下载包
    int flag;
    char buffer[1024];//存放除文件名中的内容。
    char fname[50];//如果type是1 就是文件名传输包，那么fname里面就存放着文件名
    int bytes;//这个字段用来记录传输文件的时候每个数据包实际的文件字节数
}MSG;//这个结构体会根据业务需求的不断变化，这个结构体会添加新的子段。



//根据网盘客户端的业务需求，客户端想要查看下服务器这边目录下的文件名信息，因此
//服务器就必须设计一个功能，把某个目录下的文件名信息全获取出来然后发给客户端。
//默认情况下服务器的目录我们设置为，用户的家目录 /home/chenhao
//在linux下如何对文件和目录进行读取并获取文件名
void search_server_dir(int accept_socket)
{
    //opendir是打开Linux目录的api函数
    struct dirent* dir = NULL;
    MSG info_msg = { 0 };
    int res = 0;
    DIR* dp = opendir("/home/xxwu");
    info_msg.type = MSG_TYPE_FILENAME;
    if (NULL == dp)
    {
        perror("open dir error:");
        return;
    }
    while (1)
    {
        dir = readdir(dp);
        if (NULL == dir)    //如果readdir函数返回空值，表示目录全部获取完成。
        {
            break;
        }
        if (dir->d_name[0] != '.')  //把隐藏文件过滤掉
        {
            //intf("name=%s\n", dir->d_name);
            memset(info_msg.fname, 0, sizeof(info_msg.fname));
            strcpy(info_msg.fname, dir->d_name);
            res = write(accept_socket, &info_msg, sizeof(MSG)); //把每个文件名拷贝到info_msg结构体中，通过socket发送出去
            if (res < 0)
            {
                perror("send client error:");
                return;
            }
        }
    }
}

//通过打开服务器中的某个文件，并使用socket网络发送给客户端，至于什么文件我们先定一个
void server_file_download(int accept_socket, char* back_file)
{
    MSG file_msg = { 0 };
    int res = 0;
    int fd;     //文件描述符  linux系统下很重要的概念，linux认为所有设备都是文件。对文件的打开，对设备的读写
    //都可以使用文件描述符概念
    char file_name[100] = { "/home/xxwu/" };
    strcat(file_name, back_file);
    fd = open(file_name, O_RDONLY);
    if (fd < 0)
    {
        perror("file open error");
        return;
    }
    file_msg.type = MSG_TYPE_DOWNLOAD;
    strcpy(file_msg.fname, back_file);
    //在读取文件并把文件传到客户端这个时候，MSG结构中的buffer就是存放文件的内容，但是一般来说文件都超过
    //1024字节，所以要发送多个包，
    while ((res = read(fd, file_msg.buffer, sizeof(file_msg.buffer))) > 0)       //当read用于读取文件的时候，当文件读到末尾之后read将返回小于0的值
    {       //res就是实际读取文件的字节数
        file_msg.bytes = res;
        res = write(accept_socket, &file_msg, sizeof(MSG));
        if (res < 0)
        {
            perror("server send file error:");
        }
        memset(file_msg.buffer, 0, sizeof(file_msg.buffer));
    }
}


void* thread_fun(void* arg)
{
    int acpt_socket = *((int*)arg);
    int res;
    char buffer[50] = { 0 };
    MSG recv_msg = { 0 };
    char up_file_name[20] = { 0 };
    int fd = -1;//这里定义一个文件描述符
    //read函数就是接受客户端发来的数据，返回值表示实际从客户端那边收到的字节数
    //buffer就是收到客户端数据后把数据存放的地址，sizeof(buffer)就是希望读取的字节
    //search_server_dir(acpt_socket);
    //printf("目录信息发送客户端完成！\n");
    while (1)
    {
        res = read(acpt_socket, &recv_msg, sizeof(MSG));
        if (res == 0)       //说明客户端已经断开服务器的连接了。read默认情况下他是堵塞模式
        {
            printf("客户端已经断开！\n");
            break;
        }
        if (recv_msg.type == MSG_TYPE_FILENAME) //说明客户端发过来的信息要的是目录
        {
            search_server_dir(acpt_socket);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_DOWNLOAD)
        {
            server_file_download(acpt_socket, recv_msg.fname);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_UPLOAD) //如果收到的是上传包，说明准备接收客户端发来的文件数据
        {
            //要从数据包的文件名中获取文件名信息，然后创建文件，默认创建的文件夹是在家目录下。
            strcpy(up_file_name, recv_msg.fname);   //假设up_file_name里面的信息是study.c
            //然后在家目录下创建文件
            char file_name[100] = { "/home/xxwu/" };
            printf("%s\n", up_file_name);
            strcat(file_name, up_file_name);
            fd = open(file_name, O_CREAT | O_WRONLY, 0666);
            if (fd < 0)
            {
                perror("create up file error");
            }
            //up_file_load(acpt_socket);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_UPLOAD_DATA) //如果收到的是上传包，说明准备接收客户端发来的文件数据
        {
            //写的字节数是recv_msg_bytes
            res = write(fd, recv_msg.buffer, recv_msg.bytes);
            if (recv_msg.bytes < sizeof(recv_msg.buffer))
            {
                //说明这个部分数据是文件的最后一部分数据
                printf("client up file ok\n");
                close(fd);
            }
            memset(&recv_msg, 0, sizeof(MSG));
        }
        memset(&recv_msg, 0, sizeof(MSG));

    }
    //服务器收到客户端数据后，原封不动的把数据再回发给客户端。
}

int main()
{
    char buffer[50] = { 0 };
    int res = 0;
    int server_socket;//这个是socket网络描述符，也叫套接字描述符
    int accept_socket;
    pthread_t thread_id;//线程编号
    //第一步 创建套接字描述符，买一部手机
    printf("开始创建TCP连接\n");
    server_socket = socket(AF_INET, SOCK_STREAM, 0);//我们要想向网络发送数据都使用server_socket这个套接字描述符

    if (server_socket < 0)
    {
        perror("socket create failed");
        return 0;
    }
    //第二步 告诉这个服务器，我的ip地址和端口号，我们要有一个保存ip地址和端口的变量
    struct sockaddr_in server_addr;//类似购买电话卡
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;//告诉系统自动绑定网卡ip地址
    server_addr.sin_port = htons(6666);//网络地址转换，把主机字节顺序转换成网络字节顺序

    //如果我们服务器程序退出之后，然后又立即打开服务器程序，系统就会提示Address already in use
    //这是因为ip地址和端口号是系统资源，必须把他设置为端口号可以重复使用
    int optvalue = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    //第三步 把我们设置好的id，端口号，绑定，把电话卡插到手机上
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("server bind error:");
        return 0;
    }
    //第四步，我们调用listen，开始监听程序，相当于把电话放在身上，电话铃响我就能听到声音
    if (listen(server_socket, 10) < 0)
    {
        perror("server listen error");
        return 0;
    }
    //第五步，以上四个步骤都ok后，我们就可以等待客户端连接了
    //当我们程序调用这个函数的时候，如果没有客户端连接到我们的服务器上
    //那么这个函数就将堵塞(程序在这里停了，不走了)，直到有客户端连接到服务器上，这个函数将解除堵塞
    // 并且返回一个新的套接字描述符，那么后期和客户端的通讯都将交给这个新的套接字描述符
    //负责
    printf("TCP服务器启动完毕\n");

    while (1)
    {
        accept_socket = accept(server_socket, NULL, NULL);//如果有客户端连接，就返回一个新的套接字
        printf("有客户端连接到服务器!\n");
        //创建一个新的线程，创建成功之后，系统就会执行thread_fun代码，而这里面的代码就多线程代码
        pthread_create(&thread_id, NULL, thread_fun, &accept_socket);

    }
    printf("%s 向你问好!\n", "Linux_c_simple");
    return 0;
}