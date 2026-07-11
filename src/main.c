#include "daemon.h"
#include "compiler.h"

PGOV_AUTHOR("seclususs");
PGOV_LICENSE("GPL-3.0-only");

int main(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);
	return pg_daemon_init();
}
