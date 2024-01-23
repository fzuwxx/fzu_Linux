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
    int type; //Э������ 0��ʾ��¼Э��� 1��ʾ�ļ��������
    int flag;
    char buffer[1024];//��ų��ļ���֮������ݡ�
    char fname[50];//���type��1 �����ļ������������ôfname����ʹ�����ļ���
    int bytes;//����ֶ�������¼�����ļ���ʱ��ÿ�����ݰ�ʵ�ʵ��ļ��ֽ���
}MSG;//����ṹ������ҵ������Ĳ��ϱ仯������ṹ�������µ��ӶΡ�

int fd = -1;     //������������ļ����ж�д���ļ���������Ĭ�������Ϊ0��ʾ��û�д�

void net_disk_ui()
{
    //system("clear");
    printf("========================TCP���̿ͻ���========================\n");
    printf("--------------------------���ܲ˵�---------------------------\n");
    printf("\t\t\t1.��ѯ�ļ�\n");
    printf("\t\t\t2.�����ļ�\n");
    printf("\t\t\t3.�ϴ��ļ�\n");
    printf("\t\t\t4.ˢ�½���\n");
    printf("\t\t\t5.�鿴·���ļ�\n");
    printf("\t\t\t0.�˳�ϵͳ\n");
    printf("-------------------------------------------------------------\n");
    printf("��ѡ����Ҫִ�еĲ���:\n");
}

void* thread_fun(void* arg)
{
    int res;
    int client_socket = *((int*)arg);
    MSG recv_msg = { 0 };
    while (1)
    {
        //���ڽ��ܷ������˷�����������
        res = read(client_socket, &recv_msg, sizeof(recv_msg));
        if (recv_msg.type == MSG_TYPE_FILENAME)      //�����������������������MSG_TYPE_FILENAME��ʾ������ݰ��ǰ����ļ�
        {
            printf("server path filename=%s\n", recv_msg.fname);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_DOWNLOAD)     //˵���������˷�������һ�����ļ������ý���׼��
        {
            //1.Ҫȷ��������ļ������ĸ�Ŀ¼�£����ǿ��Դ���һ��Ŀ¼mkdir����  downloadĿ¼
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
            //Ŀ¼����û����֮�󣬾�Ҫ��ʼ�����ļ�
            if (fd == -1)     //��ʾ�ļ���û�д򿪹�
            {
                //printf("current =%s\n", getcwd(NULL, 0));
                //O_CREAT �ļ������ڣ�Ҫ���´���
                char file_name[100] = { "./download/" };
                strcat(file_name, recv_msg.fname);
                fd = open(file_name, O_CREAT | O_WRONLY, 0666);       //�򿪳ɹ�֮��϶����и��ļ���������
                if (fd < 0)
                {
                    perror("file open error:");
                }
            }
            //ͨ������Ĵ���Ŀ¼���ļ����������ж�ͨ���󣬾Ϳ��Դ�MSG�ṹ�������bufferȡ������
            //recv_msg.buffer��ŵľ����ļ��Ĳ������ݣ�recv_msg.bytes������������ļ����ֽ���
            res = write(fd, recv_msg.buffer, recv_msg.bytes);
            if (res < 0)
            {
                perror("file write error:");
            }
            //��ô������ô�ж��ļ������ݶ�ȫ���������أ�����ͨ��recv_msg.bytes���С��recv_buffer��1024
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
    //�ͻ���ʵ���ϴ��ļ����������߼�˼·
    //1.�϶���Ҫ���ļ�
    MSG up_file_msg = { 0 };
    char buffer[1024] = { 0 };//���������ȡ�ļ������ݻ�����
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
    //2.��ȡ�ļ�����
    while ((res = read(fd, buffer, sizeof(buffer))) > 0)
    {
        //Ҫ���ļ��������ݿ�����MSG�ṹ���е�buffer��
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
        if (NULL == dir)    //���readdir�������ؿ�ֵ����ʾĿ¼ȫ����ȡ��ɡ�
        {
            break;
        }
        if (dir->d_name[0] != '.')  //�������ļ����˵�
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
    char c; ///���������û��ڼ�����������ַ�
    struct sockaddr_in  server_addr;  //������д��Ҫ���ӵ���������IP��ַ�Ͷ˿ں�
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
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  //����������Ϳͻ��˶�����ͬһ̨��������ôip��ַ��������Ϊ127.0.0.1
    server_addr.sin_port = htons(6666);
    //�������׽���֮���أ��ͻ���Ҫ���ӵ������������ʱ�����ӷ�����ʹ��connect

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect error!");
        return 0;
    }
    printf("�ͻ������ӷ������ɹ���\n");
    pthread_create(&thread_id, NULL, thread_fun, &client_socket);
    net_disk_ui();

    while (1)
    {

        c = getchar();
        switch (c)
        {
        case '1':
            //Ҫ�÷����������Ƿ���Ŀ¼��Ϣ
            //���whileѭ������Ҳ����ѭ������ô����Ҫ�ÿͻ���Ҳ�����̣߳��ý��ܷ����������ݵĴ���Ž��߳���
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
            //���ϴ��ļ���������֮ǰ����Ҫ�ȷ���һ�����ݰ��������������߷����������׼���ϴ��ļ���
            strcpy(send_msg.fname, up_file_name);
            res = write(client_socket, &send_msg, sizeof(MSG));
            if (res < 0)
            {
                perror("send up load packg error");
                continue;
            }
            memset(&send_msg, 0, sizeof(MSG));
            //���ڿ��ǵ��ϴ��ļ�����Ҫ�Ƚϳ���ʱ�䣬���ǵ�����ļ��ܴ���ô����Ҫ�ǳ�����ʱ�䣬���ʱ�����д
            //��������ô�Ϳ��ܵ����������ܿ����Ŷӵȴ��������Ҫ�ѷ����ļ��Ĵ���Ž��߳����棬������ǵĿͻ���
            //����Ҫ����һ���µ��̣߳���ר�Ŵ����ļ��ϴ�����
            pthread_create(&thread_send_id, NULL, upload_file_thread, &client_socket);
            break;
        case '4':
            system("clear"); // ���ҳ��
            net_disk_ui(); // ���´�ӡ�˵�
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
    //�����û��ڿͻ����ն����������ַ������س���ʾ�����ݷ��ͳ�ȥ��
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
