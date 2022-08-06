#pragma once
#include <iostream>
#include <list>
#include "utils.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <set>
#include <vector>
#include "model.h"
#include <unordered_map>

using namespace std;

const int PRINT_STATUS_INTERVAL = 10;

class Registry
{
private:
	static unordered_map<IpPort, uint64_t> m_last_ping;
	static unordered_map<IpPort, string> m_ids;
	static int m_server_fd;
	static Model *m_model;
	static vector<IpPort> m_servers, m_file_servers, m_backup_servers;
	static unordered_map<IpPort, ServerStatus> m_server_status, m_file_server_status;
	static mutex m_server_status_mtx, m_file_server_status_mtx;

public:
	Registry(int port, string db_host, int db_port, string db_user, string db_pwd, string db_name);
	static void PrintStatusThread();
	void Run();
	static void Add(Msg msg, sockaddr_in addr);
	static void Get(Msg msg, sockaddr_in addr);
	~Registry();
};