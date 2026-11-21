#include "pgov/compiler.h"
#include "pgov/daemon.h"

PGOV_AUTHOR("seclususs");
PGOV_LICENSE("GPL-3.0-only");

int main(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);
	return daemon_init();
}
