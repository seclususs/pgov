// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/prop.h"
#include "pgov/config.h"
#include "pgov/log.h"
#include <errno.h> // IWYU pragma: keep
#include <string.h>
#include <time.h>
#include <unistd.h>

static inline void maybe_delay_stabilization(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0 && ts.tv_sec < 60)
		sleep(CFG_STAB_DELAY_SEC);
}

bool pg_prop_wait_boot(void)
{
	char val[PROP_VALUE_MAX];

	for (unsigned int retries = 0; retries <= CFG_BOOT_RETRY_MAX;
	     retries++) {
		int len = __system_property_get("sys.boot_completed", val);
		if (len == 1 && val[0] == '1') {
			maybe_delay_stabilization();
			return true;
		}

		sleep(CFG_BOOT_POLL_SEC);
	}

	LOGE("prop: timeout waiting for sys.boot_completed");
	return false;
}

#ifdef NDK_BUILD

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

	if (UNLIKELY(!state || !val))
		return -EINVAL;

	int len = __system_property_get(state->name, os_val);
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
		LOGE("prop: failed to set %s to %s", state->name, val);
		return -EIO;
	}

	return 0;
}

int pg_prop_reset(struct pg_prop_state *state)
{
	if (UNLIKELY(!state || !state->modified))
		return 0;

	if (__system_property_set(state->name, state->val) != 0) {
		LOGE("prop: failed to res %s to %s", state->name, state->val);
		return -EIO;
	}

	state->modified = false;
	return 0;
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
