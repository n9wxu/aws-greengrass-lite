// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "config_path_object.h"
#include <ggl/core_bus/gg_config.h>
#include <ggl/log.h>
#include <ggl/object.h>
#include <ggl/vector.h>
#include <stddef.h>

GglError ggl_make_config_path_object(
    GglBuffer component_name, GglList key_path, GglBufList *result
) {
    static GglBuffer full_key_path_mem[GGL_MAX_CONFIG_DEPTH];
    GglBufVec full_key_path = GGL_BUF_VEC(full_key_path_mem);

    GglError ret = ggl_buf_vec_push(&full_key_path, GGL_STR("services"));
    ggl_buf_vec_chain_push(&ret, &full_key_path, component_name);
    ggl_buf_vec_chain_push(&ret, &full_key_path, GGL_STR("configuration"));
    ggl_buf_vec_chain_append_list(&ret, &full_key_path, key_path);
    if (ret != GGL_ERR_OK) {
        GGL_LOGE("config", "Key path too long.");
        return ret;
    }

    *result = full_key_path.buf_list;
    return GGL_ERR_OK;
}

GglError ggl_parse_config_path(
    GglList config_path, GglBuffer *component_name, GglList *key_path
) {
    if (config_path.len < 4) {
        GGL_LOGE("config", "Config path is not in the expected format");
        return GGL_ERR_INVALID;
    }

    *component_name = config_path.items[1].buf;

    static GglObject component_key_path_mem[GGL_MAX_COMPONENT_CONFIG_DEPTH];
    static GglObjVec component_key_path = GGL_OBJ_VEC(component_key_path_mem);
    component_key_path.list.len = 0;

    GglError ret = ggl_obj_vec_push(
        &component_key_path, GGL_OBJ(config_path.items[3].buf)
    );
    for (size_t i = 4; i < config_path.len; i++) {
        ggl_obj_vec_chain_push(
            &ret, &component_key_path, GGL_OBJ(config_path.items[i].buf)
        );
    }
    if (ret != GGL_ERR_OK) {
        GGL_LOGE("config", "Key path too long.");
        return ret;
    }

    *key_path = component_key_path.list;
    return GGL_ERR_OK;
}