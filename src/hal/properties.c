// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "daemon/properties.h"
#include "daemon/config.h"
#include <sys/system_properties.h>
#include <unistd.h>

bool pascal_gov_properties_wait_boot(void)
{
	unsigned int retry_count = 0;
	char prop_value[PROP_VALUE_MAX];

	while (retry_count <= PASCAL_GOV_BOOT_WAIT_RETRY_LIMIT) {
		int len =
			__system_property_get("sys.boot_completed", prop_value);

		if (len > 0 && prop_value[0] == '1' && prop_value[1] == '\0') {
			sleep(PASCAL_GOV_STABILIZATION_DELAY_SEC);
			return true;
		}

		retry_count++;
		sleep(PASCAL_GOV_BOOT_POLL_INTERVAL_SEC);
	}

	return false;
}
