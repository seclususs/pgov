#include "daemon.h"
#include "compiler.h"

int main(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);
	return pg_daemon_init();
}
