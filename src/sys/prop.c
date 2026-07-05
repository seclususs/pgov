// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "prop.h"
#include "pg/config.h"
#include <sys/system_properties.h>
#include <unistd.h>

bool pg_prop_wait_boot(void)
{
	unsigned int retry_count = 0;
	char prop_value[PROP_VALUE_MAX];

	while (retry_count <= PG_BOOT_RETRY_MAX) {
		int len =
			__system_property_get("sys.boot_completed", prop_value);

		if (len > 0 && prop_value[0] == '1' && prop_value[1] == '\0') {
			sleep(PG_STAB_DELAY_SEC);
			return true;
		}

		retry_count++;
		sleep(PG_BOOT_POLL_SEC);
	}

	return false;
}
