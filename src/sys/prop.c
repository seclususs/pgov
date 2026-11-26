// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#include "pgov/prop.h"
#include "pgov/log.h"
#include <errno.h> // IWYU pragma: keep
#include <string.h>
#include <time.h>
#include <unistd.h>

#define STAB_DELAY_SEC 10
#define BOOT_RETRY_MAX 300
#define BOOT_POLL_SEC 1

static inline void delay_stabilization(void)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_BOOTTIME, &ts) == 0 && ts.tv_sec < 60)
		sleep(STAB_DELAY_SEC);
}

bool prop_wait_boot(void)
{
	char val[PROP_VALUE_MAX];
	unsigned int retries;
	int len;

	for (retries = 0; retries <= BOOT_RETRY_MAX; retries++) {
		len = __system_property_get("sys.boot_completed", val);
		if (len == 1 && val[0] == '1') {
			delay_stabilization();
			return true;
		}

		sleep(BOOT_POLL_SEC);
	}

	LOGE("prop: timeout waiting for sys.boot_completed");
	return false;
}

#ifdef NDK_BUILD

void prop_init(struct prop_state *RESTRICT state, const char *RESTRICT key)
{
	size_t len;

	if (UNLIKELY(!state))
		return;

	state->key[0] = '\0';
	state->val[0] = '\0';
	state->modified = false;

	if (UNLIKELY(!key))
		return;

	len = strlen(key);
	if (UNLIKELY(len == 0 || len >= PROP_KEY_MAX))
		return;

	memcpy(state->key, key, len + 1);
}

int prop_set(struct prop_state *RESTRICT state, const char *RESTRICT val)
{
	char os_val[PROP_VALUE_MAX];
	int len;
	bool was_mod;

	if (UNLIKELY(!state || !val || state->key[0] == '\0'))
		return -EINVAL;

	len = __system_property_get(state->key, os_val);
	if (len <= 0)
		os_val[0] = '\0';

	if (strncmp(os_val, val, PROP_VALUE_MAX) == 0)
		return 0;

	was_mod = state->modified;
	if (!state->modified) {
		strncpy(state->val, os_val, PROP_VALUE_MAX - 1);
		state->val[PROP_VALUE_MAX - 1] = '\0';
		state->modified = true;
	}

	if (__system_property_set(state->key, val) != 0) {
		state->modified = was_mod;
		LOGE("prop: failed to set %s to %s", state->key, val);
		return -EIO;
	}

	return 0;
}

int prop_reset(struct prop_state *state)
{
	if (UNLIKELY(!state || !state->modified))
		return 0;

	if (__system_property_set(state->key, state->val) != 0) {
		LOGE("prop: failed to res %s to %s", state->key, state->val);
		return -EIO;
	}

	state->modified = false;
	return 0;
}

void prop_cleanup(struct prop_state *state)
{
	if (UNLIKELY(!state))
		return;

	if (state->modified)
		prop_reset(state);

	state->key[0] = '\0';
	state->val[0] = '\0';
	state->modified = false;
}

#endif // NDK_BUILD
