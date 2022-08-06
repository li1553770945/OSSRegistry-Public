#include "model.h"
#include <sys/time.h>
#include <thread>
#include <string.h>
#include "easylogging++.h"

MySqlPool *Model::m_pool;

Model::Model(string host, string user, string pwd, string dbname, int port)
{
	m_pool = new MySqlPool(host, user, pwd, dbname, port);
	m_pool->Connect();
}
string Model::GetOrAddServer(IpPort &ip)
{
	MYSQL *mysql = m_pool->GetConnection();
	char query[200];
	sprintf(query, "SELECT server_id FROM server WHERE addr='%s' AND port = '%d' AND type = 'server';", IpToDot(ip.ip).data(), ip.port);
	mysql_query(mysql, query);

	MYSQL_RES *sql_res = mysql_store_result(mysql);
	MYSQL_ROW row;
	if (nullptr == sql_res)
	{
		LOG(ERROR) << "sql store result failed." << endl;
		return "";
	}
	if (mysql_num_rows(sql_res) == 0) //新的服务器
	{
		mysql_free_result(sql_res);
		sprintf(query, "SELECT COUNT(*) FROM server WHERE type = 'server';");
		mysql_query(mysql, query);
		sql_res = mysql_store_result(mysql);
		int count = 0;
		while (row = mysql_fetch_row(sql_res))
		{
			count = atoi(row[0]) + 1;
			break;
		}
		mysql_free_result(sql_res);

		char server_id[6];
		sprintf(server_id, "1%04d", count);
		sprintf(query, "INSERT INTO server(server_id,addr,port,type) VALUES('%s','%s',%d,'server');", server_id, IpToDot(ip.ip).data(), ip.port);
		mysql_query(mysql, query);

		m_pool->RecoverConnection(mysql);
		return server_id;
	}
	else
	{
		while (row = mysql_fetch_row(sql_res)) //以前的服务器，直接返回
		{
			m_pool->RecoverConnection(mysql);
			return row[0];
		}
		return "";
	}
}
string Model::GetOrAddFileServer(IpPort &ip)
{
	MYSQL *mysql = m_pool->GetConnection();
	char query[200];
	sprintf(query, "SELECT server_id FROM server WHERE addr='%s' AND port = '%d' AND type = 'file_server';", IpToDot(ip.ip).data(), ip.port);
	mysql_query(mysql, query);

	MYSQL_RES *sql_res = mysql_store_result(mysql);
	MYSQL_ROW row;
	if (mysql_num_rows(sql_res) == 0) //新的服务器
	{
		mysql_free_result(sql_res);
		sprintf(query, "SELECT COUNT(*) FROM server WHERE type = 'file_server';");
		mysql_query(mysql, query);
		sql_res = mysql_store_result(mysql);
		int count = 0;
		while (row = mysql_fetch_row(sql_res))
		{
			count = atoi(row[0]) + 1;
			break;
		}
		mysql_free_result(sql_res);

		char server_id[6];
		sprintf(server_id, "2%04d", count);
		sprintf(query, "INSERT INTO server(server_id,addr,port,type) VALUES('%s','%s',%d,'file_server');", server_id, IpToDot(ip.ip).data(), ip.port);
		mysql_query(mysql, query);

		m_pool->RecoverConnection(mysql);
		return server_id;
	}
	else
	{
		while (row = mysql_fetch_row(sql_res)) //以前的服务器，直接返回
		{
			m_pool->RecoverConnection(mysql);
			return row[0];
		}
	}
	return "";
}
uint32_t Model::GetUserNum()
{
	MYSQL *mysql = m_pool->GetConnection();
	char query[200];
	sprintf(query, "SELECT COUNT(*) FROM user;");
	mysql_query(mysql, query);
	MYSQL_RES *sql_res = mysql_store_result(mysql);
	MYSQL_ROW row;
	while (row = mysql_fetch_row(sql_res))
	{
		m_pool->RecoverConnection(mysql);
		return atoi(row[0]);
	}
	LOG(ERROR) << "get user num return 0" << endl;
	m_pool->RecoverConnection(mysql);
	return 0;
}
Model::~Model()
{
	delete m_pool;
}
