#include <iostream>
#include "registry.h"
#include "easylogging++.h"
using namespace std;

INITIALIZE_EASYLOGGINGPP

int main(int argc, char *argv[])
{
	el::Configurations conf("./log.conf");
	el::Loggers::reconfigureAllLoggers(conf);
	Registry reg(12001, "9.135.135.245", 3306, "root", "Li5060520", "oss");
	reg.Run();
}