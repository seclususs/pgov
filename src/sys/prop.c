// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "prop.h"
#include "pg/config.h"
#include <sys/system_properties.h>
#include <unistd.h>

bool pg_prop_wait_boot(void)
{
	unsigned int retries = 0;
	char val[PROP_VALUE_MAX];

	while (retries <= PG_BOOT_RETRY_MAX) {
		int len = __system_property_get("sys.boot_completed", val);

		if (len > 0 && val[0] == '1' && val[1] == '\0') {
			sleep(PG_STAB_DELAY_SEC);
			return true;
		}

		retries++;
		sleep(PG_BOOT_POLL_SEC);
	}

	return false;
}
