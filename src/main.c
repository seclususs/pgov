#include "daemon/daemon.h"
#include "pascal_gov/compiler.h"

int main(int argc, char *argv[])
{
	PASCAL_GOV_UNUSED(argc);
	PASCAL_GOV_UNUSED(argv);
	return pascal_gov_daemon_init();
}
