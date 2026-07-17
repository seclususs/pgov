// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "prop.h"
#include "pg/config.h"
#include <errno.h> // IWYU pragma: keep
#include <string.h>
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

#if defined(NDK_BUILD)

void pg_prop_state_init(struct pg_prop_state *RESTRICT state,
			const char *RESTRICT name)
{
	if (UNLIKELY(!state || !name))
		return;

	strncpy(state->name, name, PROP_NAME_MAX - 1);
	state->name[PROP_NAME_MAX - 1] = '\0';
	state->val[0] = '\0';
	state->modified = false;
}

int pg_prop_set(struct pg_prop_state *RESTRICT state, const char *RESTRICT val)
{
	char os_val[PROP_VALUE_MAX];
	int ret = 0;
	int len;

	if (UNLIKELY(!state || !val))
		return -EINVAL;

	len = __system_property_get(state->name, os_val);
	if (len <= 0)
		os_val[0] = '\0';

	if (strncmp(os_val, val, PROP_VALUE_MAX) == 0)
		return 0;

	bool was_mod = state->modified;

	if (!state->modified) {
		strncpy(state->val, os_val, PROP_VALUE_MAX - 1);
		state->val[PROP_VALUE_MAX - 1] = '\0';
		state->modified = true;
	}

	if (__system_property_set(state->name, val) != 0) {
		state->modified = was_mod;
		ret = -EIO;
		goto out;
	}

out:
	return ret;
}

int pg_prop_reset(struct pg_prop_state *state)
{
	int ret = 0;

	if (UNLIKELY(!state || !state->modified))
		return 0;

	if (__system_property_set(state->name, state->val) != 0) {
		ret = -EIO;
		goto out;
	}

	state->modified = false;

out:
	return ret;
}

void pg_prop_cleanup(struct pg_prop_state *state)
{
	if (UNLIKELY(!state))
		return;

	if (state->modified)
		pg_prop_reset(state);

	state->name[0] = '\0';
	state->val[0] = '\0';
	state->modified = false;
}

#endif // NDK_BUILD
