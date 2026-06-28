// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 seclususs

#ifndef PASCAL_GOV_COMPILER_H
#define PASCAL_GOV_COMPILER_H

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef PASCAL_GOV_RESTRICT
#define PASCAL_GOV_RESTRICT __restrict
#endif

#ifndef PASCAL_GOV_LIKELY
#define PASCAL_GOV_LIKELY(x) __builtin_expect(!!(x), 1)
#define PASCAL_GOV_UNLIKELY(x) __builtin_expect(!!(x), 0)
#endif

#ifndef PASCAL_GOV_ALWAYS_INLINE
#define PASCAL_GOV_ALWAYS_INLINE __attribute__((always_inline))
#endif

#ifndef PASCAL_GOV_PURE
#define PASCAL_GOV_PURE __attribute__((pure))
#endif

#ifndef PASCAL_GOV_CONST
#define PASCAL_GOV_CONST __attribute__((const))
#endif

#ifndef PASCAL_GOV_ALIGNED
#define PASCAL_GOV_ALIGNED(x) __attribute__((aligned(x)))
#endif

#ifndef PASCAL_GOV_UNUSED
#define PASCAL_GOV_UNUSED(x) (void)(x)
#endif

#ifndef PASCAL_GOV_MAYBE_UNUSED
#define PASCAL_GOV_MAYBE_UNUSED __attribute__((unused))
#endif

#endif // PASCAL_GOV_COMPILER_H
