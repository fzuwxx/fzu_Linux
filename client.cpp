#include <cstdio>
#include<unistd.h>
#include<arpa/inet.h>
#include<string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#define MSG_TYPE_LOGIN       0
#define MSG_TYPE_FILENAME    1
#define MSG_TYPE_DOWNLOAD    2
#define MSG_TYPE_UPLOAD      3
#define MSG_TYPE_UPLOAD_DATA 4

char up_file_name[20] = { 0 };
char download_file[20] = { 0 };
char open_file_name[20] = { 0 };

typedef struct msg
{
    int type; //协议类型 0表示登录协议包 1表示文件名传输包
    int flag;
    char buffer[1024];//存放除文件名之外的内容。
    char fname[50];//如果type是1 就是文件名传输包，那么fname里面就存放着文件名
    int bytes;//这个字段用来记录传输文件的时候每个数据包实际的文件字节数
}MSG;//这个结构体会根据业务需求的不断变化，这个结构体会添加新的子段。

int fd = -1;     //这个是用来打开文件进行读写的文件描述符，默认情况下为0表示还没有打开

void net_disk_ui()
{
    //system("clear");
    printf("========================TCP网盘客户端========================\n");
    printf("--------------------------功能菜单---------------------------\n");
    printf("\t\t\t1.查询文件\n");
    printf("\t\t\t2.下载文件\n");
    printf("\t\t\t3.上传文件\n");
    printf("\t\t\t4.刷新界面\n");
    printf("\t\t\t5.查看路径文件\n");
    printf("\t\t\t0.退出系统\n");
    printf("-------------------------------------------------------------\n");
    printf("请选择你要执行的操作:\n");
}

void* thread_fun(void* arg)
{
    int res;
    int client_socket = *((int*)arg);
    MSG recv_msg = { 0 };
    while (1)
    {
        //用于接受服务器端发过来的数据
        res = read(client_socket, &recv_msg, sizeof(recv_msg));
        if (recv_msg.type == MSG_TYPE_FILENAME)      //如果服务器发过来的数据是MSG_TYPE_FILENAME表示这个数据包是包含文件
        {
            printf("server path filename=%s\n", recv_msg.fname);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_DOWNLOAD)     //说明服务器端发过来的一定是文件，做好接受准备
        {
            //1.要确定下这个文件放在哪个目录下，我们可以创建一个目录mkdir函数  download目录
            if (mkdir("download", S_IRWXU) < 0)
            {
                if (errno == EEXIST)
                {
                    printf("dir exist continue! \n");
                }
                else
                {
                    perror("mkdir error");
                }
            }
            //目录创建没问题之后，就要开始创建文件
            if (fd == -1)     //表示文件还没有打开过
            {
                //printf("current =%s\n", getcwd(NULL, 0));
                //O_CREAT 文件不存在，要重新创建
                char file_name[100] = { "./download/" };
                strcat(file_name, recv_msg.fname);
                fd = open(file_name, O_CREAT | O_WRONLY, 0666);       //打开成功之后肯定会有个文件描述返回
                if (fd < 0)
                {
                    perror("file open error:");
                }
            }
            //通过上面的创建目录，文件描述符的判断通过后，就可以从MSG结构体里面的buffer取数据了
            //recv_msg.buffer存放的就是文件的部分内容，recv_msg.bytes就是这个部分文件的字节数
            res = write(fd, recv_msg.buffer, recv_msg.bytes);
            if (res < 0)
            {
                perror("file write error:");
            }
            //那么我们怎么判断文件的内容都全部发完了呢？可以通过recv_msg.bytes如果小于recv_buffer的1024
            if (recv_msg.bytes < sizeof(recv_msg.buffer))
            {
                printf("file download finish!\n");
                close(fd);
                fd = -1;
            }
        }
    }

}

void* upload_file_thread(void* args)
{
    //客户端实现上传文件到服务器逻辑思路
    //1.肯定是要打开文件
    MSG up_file_msg = { 0 };
    char buffer[1024] = { 0 };//用来保存读取文件的数据缓冲区
    int client_socket = *((int*)args);
    int fd = -1;
    int res = 0;
    char file_name[100] = { "./download/" };
    strcat(file_name, up_file_name);
    fd = open(file_name, O_RDONLY);
    if (fd < 0)
    {
        perror("open up file error");
        return NULL;
    }
    up_file_msg.type = MSG_TYPE_UPLOAD_DATA;
    //2.读取文件内容
    while ((res = read(fd, buffer, sizeof(buffer))) > 0)
    {
        //要把文件数据内容拷贝到MSG结构体中的buffer中
        memcpy(up_file_msg.buffer, buffer, res);
        up_file_msg.bytes = res;
        res = write(client_socket, &up_file_msg, sizeof(MSG));
        memset(buffer, 0, sizeof(buffer));
        memset(up_file_msg.buffer, 0, sizeof(up_file_msg.buffer));
    }
}

void display_filename(char* front_name)
{
    struct dirent* dir = NULL;
    DIR* dp = opendir(front_name);
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
            printf("file name=%s\n", dir->d_name);
        }
    }
    return;
}

int main()
{
    int client_socket;
    pthread_t thread_id;
    pthread_t thread_send_id;
    char c; ///用来保存用户在键盘上输入的字符
    struct sockaddr_in  server_addr;  //用来填写你要连接到服务器的IP地址和端口号
    char buffer[50] = { 0 };
    int res;
    MSG recv_msg = { 0 };
    MSG send_msg = { 0 };
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("client socket faile:");
        return 0;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //如果服务器和客户端都是在同一台电脑中那么ip地址可以设置为127.0.0.1
    server_addr.sin_port = htons(6666);
    //创建好套接字之后呢，客户端要连接到服务器，这个时候连接服务器使用connect

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect error!");
        return 0;
    }
    printf("客户端连接服务器成功！\n");
    pthread_create(&thread_id, NULL, thread_fun, &client_socket);
    net_disk_ui();

    while (1)
    {

        c = getchar();
        switch (c)
        {
        case '1':
            //要让服务器给我们发送目录信息
            //这个while循环本身也是死循环，那么我们要让客户端也创建线程，让接受服务器的数据的代码放进线程里
            send_msg.type = MSG_TYPE_FILENAME;
            res = write(client_socket, &send_msg, sizeof(MSG));
            if (res < 0)
            {
                perror("send msg error:");
            }
            memset(&send_msg, 0, sizeof(MSG));
            break;
        case '2':
            send_msg.type = MSG_TYPE_DOWNLOAD;
            display_filename("/home/xxwu");
            printf("input download filename:\n");
            scanf("%s", download_file);
            strcpy(send_msg.fname, download_file);
            res = write(client_socket, &send_msg, sizeof(MSG));
            if (res < 0)
            {
                perror("send msg error:");
            }
            memset(&send_msg, 0, sizeof(MSG));
            break;
        case '3':
            send_msg.type = MSG_TYPE_UPLOAD;
            display_filename("./download");
            printf("input up load filename:\n");
            scanf("%s", &up_file_name);
            //在上传文件给服务器之前，你要先发送一个数据包给服务器，告诉服务器我这边准备上传文件了
            strcpy(send_msg.fname, up_file_name);
            res = write(client_socket, &send_msg, sizeof(MSG));
            if (res < 0)
            {
                perror("send up load packg error");
                continue;
            }
            memset(&send_msg, 0, sizeof(MSG));
            //由于考虑到上传文件是需要比较长的时间，考虑到如果文件很大，那么就需要非常长的时间，这个时候如果写
            //在这里那么就可能导致其他功能卡主排队等待，因此需要把发送文件的代码放进线程里面，因此我们的客户端
            //还需要创建一个新的线程，来专门处理文件上传任务。
            pthread_create(&thread_send_id, NULL, upload_file_thread, &client_socket);
            break;
        case '4':
            system("clear"); // 清空页面
            net_disk_ui(); // 重新打印菜单
            break;
        case '5':
            printf("input open file folder name:\n");
            scanf("%s", &open_file_name);
            display_filename(open_file_name);
            break;
        case '0':
            return 0;
        }
    }
    //我们用户在客户端终端连接输入字符串，回车表示把数据发送出去。
    /*
    while (fgets(buffer, sizeof(buffer), stdin) != NULL)
    {
       res = write(client_socket, buffer, sizeof(buffer));
       printf("send bytes =%d\n", res);
       memset(buffer, 0, sizeof(buffer));
       res = read(client_socket, buffer, sizeof(buffer));
       printf("receive from server info: %s\n", buffer);
       memset(buffer, 0, sizeof(buffer));
    }*/
    return 0;
}
