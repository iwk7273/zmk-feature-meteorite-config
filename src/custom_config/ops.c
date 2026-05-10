/*
 * Copyright (c) 2025 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "internal.h"

#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#include <dt-bindings/zmk/custom_config.h>
#include <zephyr/sys/util.h>

#define CUSTOM_CONFIG_LIMIT_NONE 0
#define CUSTOM_CONFIG_LIMIT_LAYER_COUNT UINT8_MAX

enum custom_config_op_kind {
    CUSTOM_CONFIG_OP_KIND_WRAP_INC,
    CUSTOM_CONFIG_OP_KIND_WRAP_DEC,
    CUSTOM_CONFIG_OP_KIND_TOGGLE,
    CUSTOM_CONFIG_OP_KIND_SET,
    CUSTOM_CONFIG_OP_KIND_NOOP,
    CUSTOM_CONFIG_OP_KIND_ACTION,
};

struct custom_config_op_spec {
    uint8_t op;
    const char *name;
    enum custom_config_op_kind kind;
    size_t field_offset;
    uint8_t limit;
    uint8_t value;
};

#define CUSTOM_CONFIG_OP(id, code, name, kind, field, limit, value, metadata, display)             \
    BUILD_ASSERT(id == code, "custom config op code mismatch: " #id);
#include <zmk/custom_config_ops.def>
#undef CUSTOM_CONFIG_OP

#define CUSTOM_CONFIG_OP_SPEC_WRAP_INC(id, name, field, limit, value)                             \
    {                                                                                             \
        .op = id, .name = name, .kind = CUSTOM_CONFIG_OP_KIND_WRAP_INC,                           \
        .field_offset = offsetof(struct zmk_custom_config, field), .limit = limit,                \
        .value = value,                                                                           \
    }
#define CUSTOM_CONFIG_OP_SPEC_WRAP_DEC(id, name, field, limit, value)                             \
    {                                                                                             \
        .op = id, .name = name, .kind = CUSTOM_CONFIG_OP_KIND_WRAP_DEC,                           \
        .field_offset = offsetof(struct zmk_custom_config, field), .limit = limit,                \
        .value = value,                                                                           \
    }
#define CUSTOM_CONFIG_OP_SPEC_TOGGLE(id, name, field, limit, value)                               \
    {                                                                                             \
        .op = id, .name = name, .kind = CUSTOM_CONFIG_OP_KIND_TOGGLE,                             \
        .field_offset = offsetof(struct zmk_custom_config, field), .limit = limit,                \
        .value = value,                                                                           \
    }
#define CUSTOM_CONFIG_OP_SPEC_SET(id, name, field, limit, value)                                  \
    {                                                                                             \
        .op = id, .name = name, .kind = CUSTOM_CONFIG_OP_KIND_SET,                                \
        .field_offset = offsetof(struct zmk_custom_config, field), .limit = limit,                \
        .value = value,                                                                           \
    }
#define CUSTOM_CONFIG_OP_SPEC_NOOP(id, name, field, limit, value)                                 \
    {                                                                                             \
        .op = id, .name = name, .kind = CUSTOM_CONFIG_OP_KIND_NOOP, .field_offset = 0,            \
        .limit = limit, .value = value,                                                           \
    }
#define CUSTOM_CONFIG_OP_SPEC_ACTION(id, name, field, limit, value)                               \
    {                                                                                             \
        .op = id, .name = name, .kind = CUSTOM_CONFIG_OP_KIND_ACTION, .field_offset = 0,          \
        .limit = limit, .value = value,                                                           \
    }
#define CUSTOM_CONFIG_OP(id, code, name, kind, field, limit, value, metadata, display)            \
    CUSTOM_CONFIG_OP_SPEC_##kind(id, name, field, limit, value),
static const struct custom_config_op_spec custom_config_ops[] = {
#include <zmk/custom_config_ops.def>
};
#undef CUSTOM_CONFIG_OP
#undef CUSTOM_CONFIG_OP_SPEC_ACTION
#undef CUSTOM_CONFIG_OP_SPEC_NOOP
#undef CUSTOM_CONFIG_OP_SPEC_SET
#undef CUSTOM_CONFIG_OP_SPEC_TOGGLE
#undef CUSTOM_CONFIG_OP_SPEC_WRAP_DEC
#undef CUSTOM_CONFIG_OP_SPEC_WRAP_INC

static void custom_config_wrap_inc(uint8_t *value, uint8_t max) {
    *value = (*value + 1) % max;
}

static void custom_config_wrap_dec(uint8_t *value, uint8_t max) {
    *value = (*value + max - 1) % max;
}

static const struct custom_config_op_spec *custom_config_find_op(uint8_t op) {
    for (size_t i = 0; i < ARRAY_SIZE(custom_config_ops); i++) {
        if (custom_config_ops[i].op == op) {
            return &custom_config_ops[i];
        }
    }

    return NULL;
}

static const char *custom_config_op_name(const struct custom_config_op_spec *spec) {
    return spec == NULL ? "CUSTOM_CFG_UNKNOWN" : spec->name;
}

static uint8_t custom_config_op_limit(const struct custom_config_op_spec *spec) {
    if (spec->limit == CUSTOM_CONFIG_LIMIT_LAYER_COUNT) {
        return zmk_custom_config_layer_count();
    }

    return spec->limit;
}

static uint8_t *custom_config_op_field(struct zmk_custom_config *cfg,
                                       const struct custom_config_op_spec *spec) {
    return (uint8_t *)cfg + spec->field_offset;
}

int zmk_custom_config_apply_op(uint8_t op) {
    const struct custom_config_op_spec *spec = custom_config_find_op(op);
    if (spec == NULL) {
        return -ENOTSUP;
    }

    struct zmk_custom_config next = *zmk_custom_config_get();
    uint8_t *field = custom_config_op_field(&next, spec);

    switch (spec->kind) {
    case CUSTOM_CONFIG_OP_KIND_WRAP_INC:
    case CUSTOM_CONFIG_OP_KIND_WRAP_DEC: {
        uint8_t limit = custom_config_op_limit(spec);
        if (limit == 0) {
            return -EINVAL;
        }
        if (spec->kind == CUSTOM_CONFIG_OP_KIND_WRAP_INC) {
            custom_config_wrap_inc(field, limit);
        } else {
            custom_config_wrap_dec(field, limit);
        }
        break;
    }
    case CUSTOM_CONFIG_OP_KIND_TOGGLE:
        *field ^= 1;
        break;
    case CUSTOM_CONFIG_OP_KIND_SET:
        *field = spec->value;
        break;
    case CUSTOM_CONFIG_OP_KIND_NOOP:
        break;
    case CUSTOM_CONFIG_OP_KIND_ACTION:
        switch (op) {
        case C_RESET:
            zmk_custom_config_set_defaults(&next);
            break;
        case C_SAVE:
            zmk_custom_config_log("C_SAVE", zmk_custom_config_get());
            return zmk_custom_config_save();
        default:
            return -ENOTSUP;
        }
        break;
    default:
        return -ENOTSUP;
    }

    zmk_custom_config_sanitize_layers(&next);
    return zmk_custom_config_set_with_tag(&next, custom_config_op_name(spec));
}
