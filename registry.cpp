#include "iostream"
#include "registry.h"
#include <unistd.h>
#include <string.h>
#include <algorithm>
#include "easylogging++.h"
#include "model.h"
#include <vector>
using namespace std;

unordered_map<IpPort, uint64_t> Registry::m_last_ping;
unordered_map<IpPort, string> Registry::m_ids;

const int TIME_OUT = 15; //服务器超时时间
int Registry::m_server_fd;
vector<IpPort> Registry::m_servers, Registry::m_file_servers, Registry::m_backup_servers;
Model *Registry::m_model;
unordered_map<IpPort, ServerStatus> Registry::m_server_status, Registry::m_file_server_status;
mutex Registry::m_server_status_mtx, Registry::m_file_server_status_mtx;
Registry::Registry(int port, string db_host, int db_port, string db_user, string db_pwd, string db_name)
{

    struct sockaddr_in m_ser_addr;
    m_server_fd = socket(AF_INET, SOCK_DGRAM, 0); // AF_INET:IPV4;SOCK_DGRAM:UDP
    if (m_server_fd < 0)
    {
        LOG(ERROR) << "Create socket fail!" << endl;
        exit(0);
    }
    memset(&m_ser_addr, 0, sizeof(m_ser_addr));
    m_ser_addr.sin_family = AF_INET;
    m_ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    m_ser_addr.sin_port = htons(port);

    int ret = bind(m_server_fd, (struct sockaddr *)&m_ser_addr, sizeof(m_ser_addr));
    if (ret < 0)
    {
        LOG(ERROR) << "socket bind fail!\n";
        exit(0);
    }
    else
    {
        LOG(INFO) << "socket bind success!" << endl;
    }
    m_model = new Model(db_host, db_user, db_pwd, db_name, db_port);
    thread t(PrintStatusThread);
    t.detach();
}
void Registry::Run()
{
    socklen_t len;
    int count;

    Msg echo_response;
    echo_response.type = Types::EchoResponse;

    Msg msg;
    LOG(INFO) << "runing..." << endl;
    while (1)
    {
        sockaddr_in client_addr; // clent_addr用于记录发送方的地址信息
        len = sizeof(client_addr);
        count = recvfrom(m_server_fd, &msg, sizeof(msg), 0, (struct sockaddr *)&client_addr, &len); // recvfrom��ӵ��������û�����ݾ�һֱӵ��
        if (count > 0)
        {
            switch (msg.type)
            {
            case Types::EchoRequest:
            {
                LOG(INFO) << IpToDot(htonl(client_addr.sin_addr.s_addr)) << " echo" << endl;
                echo_response.id = msg.id;
                sendto(m_server_fd, &echo_response, sizeof(echo_response), 0, (struct sockaddr *)&client_addr, sizeof(client_addr));
                break;
            }
            case Types::AddServerRequest:
            case Types::AddFileServerRequest:
            case Types::AddBackupServerRequest:
            {

                Add(msg, client_addr);
                break;
            }
            case Types::GetServerRequest:
            case Types::GetFileServerRequest:
            case Types::GetBackupServerRequest:
            {
                Get(msg, client_addr);
                break;
            }
            default:
                LOG(WARNING) << "unknown msg type:" << (int)msg.type << endl;
            }
        }
        else
        {
            // TODO:没有处理接收失败的情况
            LOG(ERROR) << "recvfrom error!"
                       << " errno:" << errno << endl;
        }
    }
}

void Registry::Add(Msg msg, sockaddr_in addr)
{
    static timeval tv;
    static Msg add_server_response, add_file_server_response, add_backup_server_response;
    add_server_response.type = Types::AddServerResponse;
    add_file_server_response.type = Types::AddFileServerResponse;
    add_backup_server_response.type = Types::AddFileServerResponse;

    IpPort *ip = (IpPort *)msg.data;
    gettimeofday(&tv, NULL);

    switch (msg.type)
    {
    case Types::AddServerRequest:
    {

        if (m_last_ping[*ip] == 0)
        {
            LOG(INFO) << "server add:" << IpToDot(ip->ip) << ":" << ip->port;
            m_ids[*ip] = m_model->GetOrAddServer(*ip).data();
            m_last_ping[*ip] = tv.tv_sec;
            m_servers.emplace_back(*ip);
            strcpy(add_server_response.data, m_ids[*ip].data());
        }
        else
        {
            m_last_ping[*ip] = tv.tv_sec;
            strcpy(add_server_response.data, m_ids[*ip].data());
        }
        add_server_response.id = msg.id;
        sendto(m_server_fd, &add_server_response, sizeof(add_server_response), 0, (sockaddr *)&addr, sizeof(sockaddr));
        m_server_status_mtx.lock();
        m_server_status[*ip] = *(ServerStatus *)(msg.data + sizeof(IpPort));
        m_server_status_mtx.unlock();
        break;
    }
    case Types::AddFileServerRequest:
    {

        if (m_last_ping[*ip] == 0)
        {
            LOG(INFO) << "file server add:" << IpToDot(ip->ip) << ":" << ip->port;
            m_ids[*ip] = m_model->GetOrAddFileServer(*ip).data();
            m_last_ping[*ip] = tv.tv_sec;
            m_file_servers.emplace_back(*ip);
            strcpy(add_file_server_response.data, m_ids[*ip].data());
        }
        else
        {
            m_last_ping[*ip] = tv.tv_sec;
            strcpy(add_file_server_response.data, m_ids[*ip].data());
        }
        add_file_server_response.id = msg.id;
        sendto(m_server_fd, &add_file_server_response, sizeof(add_file_server_response), 0, (sockaddr *)&addr, sizeof(sockaddr));
        m_file_server_status_mtx.lock();
        m_file_server_status[*ip] = *(ServerStatus *)(msg.data + sizeof(IpPort));
        m_file_server_status_mtx.unlock();
        break;
    }
    default:
        break;
    }
}
void Registry::Get(Msg msg, sockaddr_in addr) //ַ查询一个服务器
{
    static struct timeval tv;
    gettimeofday(&tv, NULL);
    switch (msg.type)
    {
    case Types::GetServerRequest:
    {
        LOG(INFO) << IpToDot(htonl(addr.sin_addr.s_addr)) << " get server" << endl;
        Msg response;
        if (m_servers.size() == 0)
        {
            response.type = Types::Error;
            sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr)); //没有服务器可以用了
            return;
        }

        response.type = Types::GetServerResponse;
        response.id = msg.id;

        IpPort *ip = (IpPort *)response.data;

        int index;
        while (true)
        {
            if (m_servers.size() == 0)
            {
                response.type = Types::Error;
                sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr));
                return;
            }
            index = rand() % m_servers.size();
            if (tv.tv_sec - m_last_ping[m_servers[index]] > TIME_OUT)
            {
                LOG(WARNING) << "Server " << m_ids[m_servers[index]] << " "
                             << IpToDot(m_servers[index].ip) << ":" << m_servers[index].port
                             << " heart beat timeout,remove" << endl;
                m_last_ping.erase(m_servers[index]);
                m_ids.erase(m_servers[index]);
                m_servers.erase(m_servers.begin() + index);
            }
            else
            {
                break;
            }
        }

        ip->ip = m_servers[index].ip;
        ip->port = m_servers[index].port;

        sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr));
        break;
    }
    case Types::GetBackupServerRequest:
    case Types::GetFileServerRequest:
    {
        if (msg.type == Types::GetBackupServerRequest)
        {
            LOG(INFO) << IpToDot(htonl(addr.sin_addr.s_addr)) << " get backup server" << endl;
        }
        else
        {
            LOG(INFO) << IpToDot(htonl(addr.sin_addr.s_addr)) << " get file server" << endl;
        }

        Msg response;
        if (m_file_servers.size() == 0)
        {
            response.type = Types::Error;
            sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr));
            break;
        }
        if (msg.type == Types::GetBackupServerRequest)
        {
            response.type = Types::GetBackupServerResponse;
        }
        else
        {
            response.type = Types::GetFileServerResponse;
        }

        response.id = msg.id;

        IpPort *ip = (IpPort *)response.data;

        int index;
        while (true)
        {
            if (m_file_servers.size() == 0)
            {
                response.type = Types::Error;
                sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr));
                return;
            }
            index = rand() % m_file_servers.size();
            if (msg.type == Types::GetBackupServerRequest) //如果是要备份服务器
            {
                string machine_id = msg.data;
                if (m_ids[m_file_servers[index]] == machine_id) //正好是自己
                {
                    if (m_file_servers.size() <= 1) //只有自己
                    {
                        response.type = Types::Error;
                        sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr));
                        return;
                    }
                    index += 1;
                    index = index % m_file_servers.size(); //向后移动index
                }
            }

            if (tv.tv_sec - m_last_ping[m_file_servers[index]] > TIME_OUT)
            {
                LOG(WARNING) << "Server " << m_ids[m_file_servers[index]] << " "
                             << IpToDot(m_file_servers[index].ip) << ":" << m_file_servers[index].port
                             << " heart beat timeout,remove" << endl;
                m_last_ping.erase(m_file_servers[index]);
                m_ids.erase(m_file_servers[index]);
                m_file_servers.erase(m_file_servers.begin() + index);
            }
            else
            {
                break;
            }
        }

        ip->ip = m_file_servers[index].ip;
        ip->port = m_file_servers[index].port;

        sendto(m_server_fd, &response, sizeof(response), 0, (struct sockaddr *)&addr, sizeof(addr));
        break;
    }

    default:
        break;
    }
}
void Registry::PrintStatusThread()
{
    struct tm *ptm;
    long ts;
    int y, m, d, h, n, s;

    while (true)
    {

        cout << "----------------------------------" << endl;

        ts = time(NULL);
        ptm = localtime(&ts);
        y = ptm->tm_year + 1900; //年
        m = ptm->tm_mon + 1;     //月
        d = ptm->tm_mday;        //日
        h = ptm->tm_hour;        //时
        n = ptm->tm_min;         //分
        s = ptm->tm_sec;         //秒
        printf("%4d-%02d-%02d %02d:%02d:%02d\n", y, m, d, h, n, s);

        cout << "user num:" << m_model->GetUserNum() << endl
             << endl;
        for (auto &[ip, status] : m_server_status)
        {

            cout << "server:" << m_ids[ip] << endl;
            cout << "cpu use:" << status.cpu_rate << "%" << endl;
            cout << "memory use:" << status.free_memory * 100 / status.total_memory << "%" << endl;
            cout << "tcp num:" << status.tcp_num << endl;
            cout << "action num:" << status.action_num << endl
                 << endl;
        }

        for (auto &[ip, status] : m_file_server_status)
        {

            cout << "file server:" << m_ids[ip] << endl;
            cout << "cpu use:" << status.cpu_rate << "%" << endl;
            cout << "memory use:" << status.free_memory * 100 / status.total_memory << "%" << endl;
            cout << "total disk:" << status.total_disk << " free disk:" << status.free_disk << " free disk rate:" << status.free_disk * 100 / status.total_disk << "%" << endl;
            cout << "tcp num:" << status.tcp_num << endl;
            cout << "action num:" << status.action_num << endl;
        }
        cout << "----------------------------------" << endl;
        sleep(PRINT_STATUS_INTERVAL);
    }
}
Registry::~Registry()
{
    delete m_model;
}