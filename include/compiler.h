// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PGOV_COMPILER_H
#define PGOV_COMPILER_H

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef RESTRICT
#define RESTRICT __restrict
#endif

#ifndef LIKELY
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE inline __attribute__((always_inline))
#endif

#ifndef PURE
#define PURE __attribute__((pure))
#endif

#ifndef CONST
#define CONST __attribute__((const))
#endif

#ifndef ALIGNED
#define ALIGNED(x) __attribute__((aligned(x)))
#endif

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#ifndef MAYBE_UNUSED
#define MAYBE_UNUSED __attribute__((unused))
#endif

#endif // PGOV_COMPILER_H
