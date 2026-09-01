// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Shannon Smith

#ifndef CALENDAR_PLUS_INTEGER_MATH_H
#define CALENDAR_PLUS_INTEGER_MATH_H

#include <infiltratr/arithmetic.h>
#include <limits.h>
#include <stdint.h>

static inline int64_t
calendar_plus_floor_divide(int64_t value,
                           int64_t divisor)
{
    int64_t quotient = 0;

    return infiltratr_i64_floor_divmod(value, divisor, &quotient, NULL) ?
        quotient : 0;
}

static inline int64_t
calendar_plus_positive_modulo(int64_t value,
                              int64_t modulus)
{
    int64_t remainder = 0;

    return infiltratr_i64_floor_divmod(value, modulus, NULL, &remainder) ?
        remainder : 0;
}

static inline int64_t
calendar_plus_i64_add_saturating(int64_t left,
                                 int64_t right)
{
    return infiltratr_i64_add_saturating(left, right);
}

static inline int64_t
calendar_plus_i64_multiply_saturating(int64_t left,
                                      int64_t right)
{
    int64_t result = 0;

    if (infiltratr_i64_multiply_checked(left, right, &result))
        return result;
    return ((left < 0) != (right < 0)) ? INT64_MIN : INT64_MAX;
}

#endif
