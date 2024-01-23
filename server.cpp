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
    int type; //Э������ 0��ʾ��¼Э��� 1��ʾ�ļ����������2.�ļ����ذ�
    int flag;
    char buffer[1024];//��ų��ļ����е����ݡ�
    char fname[50];//���type��1 �����ļ������������ôfname����ʹ�����ļ���
    int bytes;//����ֶ�������¼�����ļ���ʱ��ÿ�����ݰ�ʵ�ʵ��ļ��ֽ���
}MSG;//����ṹ������ҵ������Ĳ��ϱ仯������ṹ�������µ��ӶΡ�



//�������̿ͻ��˵�ҵ�����󣬿ͻ�����Ҫ�鿴�·��������Ŀ¼�µ��ļ�����Ϣ�����
//�������ͱ������һ�����ܣ���ĳ��Ŀ¼�µ��ļ�����Ϣȫ��ȡ����Ȼ�󷢸��ͻ��ˡ�
//Ĭ������·�������Ŀ¼��������Ϊ���û��ļ�Ŀ¼ /home/chenhao
//��linux����ζ��ļ���Ŀ¼���ж�ȡ����ȡ�ļ���
void search_server_dir(int accept_socket)
{
    //opendir�Ǵ�LinuxĿ¼��api����
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
        if (NULL == dir)    //���readdir�������ؿ�ֵ����ʾĿ¼ȫ����ȡ��ɡ�
        {
            break;
        }
        if (dir->d_name[0] != '.')  //�������ļ����˵�
        {
            //intf("name=%s\n", dir->d_name);
            memset(info_msg.fname, 0, sizeof(info_msg.fname));
            strcpy(info_msg.fname, dir->d_name);
            res = write(accept_socket, &info_msg, sizeof(MSG)); //��ÿ���ļ���������info_msg�ṹ���У�ͨ��socket���ͳ�ȥ
            if (res < 0)
            {
                perror("send client error:");
                return;
            }
        }
    }
}

//ͨ���򿪷������е�ĳ���ļ�����ʹ��socket���緢�͸��ͻ��ˣ�����ʲô�ļ������ȶ�һ��
void server_file_download(int accept_socket, char* back_file)
{
    MSG file_msg = { 0 };
    int res = 0;
    int fd;     //�ļ�������  linuxϵͳ�º���Ҫ�ĸ��linux��Ϊ�����豸�����ļ������ļ��Ĵ򿪣����豸�Ķ�д
    //������ʹ���ļ�����������
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
    //�ڶ�ȡ�ļ������ļ������ͻ������ʱ��MSG�ṹ�е�buffer���Ǵ���ļ������ݣ�����һ����˵�ļ�������
    //1024�ֽڣ�����Ҫ���Ͷ������
    while ((res = read(fd, file_msg.buffer, sizeof(file_msg.buffer))) > 0)       //��read���ڶ�ȡ�ļ���ʱ�򣬵��ļ�����ĩβ֮��read������С��0��ֵ
    {       //res����ʵ�ʶ�ȡ�ļ����ֽ���
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
    int fd = -1;//���ﶨ��һ���ļ�������
    //read�������ǽ��ܿͻ��˷��������ݣ�����ֵ��ʾʵ�ʴӿͻ����Ǳ��յ����ֽ���
    //buffer�����յ��ͻ������ݺ�����ݴ�ŵĵ�ַ��sizeof(buffer)����ϣ����ȡ���ֽ�
    //search_server_dir(acpt_socket);
    //printf("Ŀ¼��Ϣ���Ϳͻ�����ɣ�\n");
    while (1)
    {
        res = read(acpt_socket, &recv_msg, sizeof(MSG));
        if (res == 0)       //˵���ͻ����Ѿ��Ͽ��������������ˡ�readĬ����������Ƕ���ģʽ
        {
            printf("�ͻ����Ѿ��Ͽ���\n");
            break;
        }
        if (recv_msg.type == MSG_TYPE_FILENAME) //˵���ͻ��˷���������ϢҪ����Ŀ¼
        {
            search_server_dir(acpt_socket);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_DOWNLOAD)
        {
            server_file_download(acpt_socket, recv_msg.fname);
            memset(&recv_msg, 0, sizeof(MSG));
        }
        else if (recv_msg.type == MSG_TYPE_UPLOAD) //����յ������ϴ�����˵��׼�����տͻ��˷������ļ�����
        {
            //Ҫ�����ݰ����ļ����л�ȡ�ļ�����Ϣ��Ȼ�󴴽��ļ���Ĭ�ϴ������ļ������ڼ�Ŀ¼�¡�
            strcpy(up_file_name, recv_msg.fname);   //����up_file_name�������Ϣ��study.c
            //Ȼ���ڼ�Ŀ¼�´����ļ�
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
        else if (recv_msg.type == MSG_TYPE_UPLOAD_DATA) //����յ������ϴ�����˵��׼�����տͻ��˷������ļ�����
        {
            //д���ֽ�����recv_msg_bytes
            res = write(fd, recv_msg.buffer, recv_msg.bytes);
            if (recv_msg.bytes < sizeof(recv_msg.buffer))
            {
                //˵����������������ļ������һ��������
                printf("client up file ok\n");
                close(fd);
            }
            memset(&recv_msg, 0, sizeof(MSG));
        }
        memset(&recv_msg, 0, sizeof(MSG));

    }
    //�������յ��ͻ������ݺ�ԭ�ⲻ���İ������ٻط����ͻ��ˡ�
}

int main()
{
    char buffer[50] = { 0 };
    int res = 0;
    int server_socket;//�����socket������������Ҳ���׽���������
    int accept_socket;
    pthread_t thread_id;//�̱߳��
    //��һ�� �����׽�������������һ���ֻ�
    printf("��ʼ����TCP����\n");
    server_socket = socket(AF_INET, SOCK_STREAM, 0);//����Ҫ�������緢�����ݶ�ʹ��server_socket����׽���������

    if (server_socket < 0)
    {
        perror("socket create failed");
        return 0;
    }
    //�ڶ��� ����������������ҵ�ip��ַ�Ͷ˿ںţ�����Ҫ��һ������ip��ַ�Ͷ˿ڵı���
    struct sockaddr_in server_addr;//���ƹ���绰��
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;//����ϵͳ�Զ�������ip��ַ
    server_addr.sin_port = htons(6666);//�����ַת�����������ֽ�˳��ת���������ֽ�˳��

    //������Ƿ����������˳�֮��Ȼ���������򿪷���������ϵͳ�ͻ���ʾAddress already in use
    //������Ϊip��ַ�Ͷ˿ں���ϵͳ��Դ�������������Ϊ�˿ںſ����ظ�ʹ��
    int optvalue = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optvalue, sizeof(optvalue));

    //������ ���������úõ�id���˿ںţ��󶨣��ѵ绰���嵽�ֻ���
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("server bind error:");
        return 0;
    }
    //���Ĳ������ǵ���listen����ʼ���������൱�ڰѵ绰�������ϣ��绰�����Ҿ�����������
    if (listen(server_socket, 10) < 0)
    {
        perror("server listen error");
        return 0;
    }
    //���岽�������ĸ����趼ok�����ǾͿ��Եȴ��ͻ���������
    //�����ǳ���������������ʱ�����û�пͻ������ӵ����ǵķ�������
    //��ô��������ͽ�����(����������ͣ�ˣ�������)��ֱ���пͻ������ӵ��������ϣ�����������������
    // ���ҷ���һ���µ��׽�������������ô���ںͿͻ��˵�ͨѶ������������µ��׽���������
    //����
    printf("TCP�������������\n");

    while (1)
    {
        accept_socket = accept(server_socket, NULL, NULL);//����пͻ������ӣ��ͷ���һ���µ��׽���
        printf("�пͻ������ӵ�������!\n");
        //����һ���µ��̣߳������ɹ�֮��ϵͳ�ͻ�ִ��thread_fun���룬��������Ĵ���Ͷ��̴߳���
        pthread_create(&thread_id, NULL, thread_fun, &accept_socket);

    }
    printf("%s �����ʺ�!\n", "Linux_c_simple");
    return 0;
}