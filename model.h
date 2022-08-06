#pragma once

#include <iostream>
#include <list>
#include <semaphore.h>
#include <mutex>
#include <queue>
#include "utils.h"
#include <mysql/mysql.h>
#include "database.h"

class Model
{
private:
	static MySqlPool *m_pool;

public:
	Model(string host, string user, string pwd, string dbname, int port);
	string GetOrAddServer(IpPort &ip); //返回的是server_id
	string GetOrAddFileServer(IpPort &ip);
	uint32_t GetUserNum();
	~Model();
};