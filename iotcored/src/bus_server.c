// aws-greengrass-lite - AWS IoT Greengrass runtime for constrained devices
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "bus_server.h"
#include "iotcored.h"
#include "mqtt.h"
#include "subscription_dispatch.h"
#include <ggl/buffer.h>
#include <ggl/core_bus/server.h>
#include <ggl/error.h>
#include <ggl/log.h>
#include <ggl/map.h>
#include <ggl/object.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

static void rpc_publish(void *ctx, GglMap params, uint32_t handle);
static void rpc_subscribe(void *ctx, GglMap params, uint32_t handle);

void iotcored_start_server(IotcoredArgs *args) {
    GglRpcMethodDesc handlers[] = {
        { GGL_STR("publish"), false, rpc_publish, NULL },
        { GGL_STR("subscribe"), true, rpc_subscribe, NULL },
    };
    size_t handlers_len = sizeof(handlers) / sizeof(handlers[0]);

    GglBuffer interface = GGL_STR("aws_iot_mqtt");

    if (args->interface_name != NULL) {
        interface = (GglBuffer) { .data = (uint8_t *) args->interface_name,
                                  .len = strlen(args->interface_name) };
    }
    GglError ret = ggl_listen(interface, handlers, handlers_len);

    GGL_LOGE("Exiting with error %u.", (unsigned) ret);
}

static void rpc_publish(void *ctx, GglMap params, uint32_t handle) {
    (void) ctx;

    GGL_LOGD("Handling publish request.");

    GglObject *topic_obj;
    GglObject *payload_obj;
    GglObject *qos_obj;
    GglError ret = ggl_map_validate(
        params,
        GGL_MAP_SCHEMA(
            { GGL_STR("topic"), true, GGL_TYPE_BUF, &topic_obj },
            { GGL_STR("payload"), false, GGL_TYPE_BUF, &payload_obj },
            { GGL_STR("qos"), false, GGL_TYPE_I64, &qos_obj },
        )
    );
    if (ret != GGL_ERR_OK) {
        GGL_LOGE("Publish received invalid arguments.");
        ggl_return_err(handle, GGL_ERR_INVALID);
        return;
    }

    IotcoredMsg msg = { .topic = topic_obj->buf, .payload = { 0 } };

    if (msg.topic.len > UINT16_MAX) {
        GGL_LOGE("Publish topic too large.");
        ggl_return_err(handle, GGL_ERR_RANGE);
        return;
    }

    if (payload_obj != NULL) {
        msg.payload = payload_obj->buf;
    }

    uint8_t qos = 0;

    if (qos_obj != NULL) {
        int64_t qos_val = qos_obj->i64;
        if ((qos_val < 0) || (qos_val > 2)) {
            GGL_LOGE("Publish received QoS out of range.");
            ggl_return_err(handle, GGL_ERR_INVALID);
            return;
        }
        qos = (uint8_t) qos_val;
    }

    ret = iotcored_mqtt_publish(&msg, qos);
    if (ret != GGL_ERR_OK) {
        ggl_return_err(handle, ret);
        return;
    }

    ggl_respond(handle, GGL_OBJ_NULL());
}

static void sub_close_callback(void *ctx, uint32_t handle) {
    (void) ctx;
    iotcored_unregister_subscriptions(handle);
}

static void rpc_subscribe(void *ctx, GglMap params, uint32_t handle) {
    (void) ctx;

    GGL_LOGD("Handling subscribe request.");

    static GglBuffer topic_filters[GGL_MQTT_MAX_SUBSCRIBE_FILTERS] = { 0 };
    size_t topic_filter_count = 0;

    GglObject *val;
    if (!ggl_map_get(params, GGL_STR("topic_filter"), &val)) {
        GGL_LOGE("Subscribe received invalid arguments.");
        ggl_return_err(handle, GGL_ERR_INVALID);
        return;
    }

    if (val->type == GGL_TYPE_BUF) {
        topic_filters[0] = val->buf;
        topic_filter_count = 1;
    } else if (val->type == GGL_TYPE_LIST) {
        GglList arg_filters = val->list;
        if (arg_filters.len == 0) {
            GGL_LOGE("Subscribe must have at least one topic filter.");
            ggl_return_err(handle, GGL_ERR_INVALID);
            return;
        }
        if (arg_filters.len > GGL_MQTT_MAX_SUBSCRIBE_FILTERS) {
            GGL_LOGE("Subscribe received more topic filters than supported.");
            ggl_return_err(handle, GGL_ERR_UNSUPPORTED);
            return;
        }

        topic_filter_count = arg_filters.len;
        for (size_t i = 0; i < arg_filters.len; i++) {
            if (arg_filters.items[i].type != GGL_TYPE_BUF) {
                GGL_LOGE("Subscribe received invalid arguments.");
                ggl_return_err(handle, GGL_ERR_INVALID);
                return;
            }
            topic_filters[i] = arg_filters.items[i].buf;
        }
    } else {
        GGL_LOGE("Subscribe received invalid arguments.");
        ggl_return_err(handle, GGL_ERR_INVALID);
        return;
    }

    for (size_t i = 0; i < topic_filter_count; i++) {
        if (topic_filters[i].len > UINT16_MAX) {
            GGL_LOGE("Topic filter too large.");
            ggl_return_err(handle, GGL_ERR_RANGE);
            return;
        }
    }

    uint8_t qos = 0;
    if (ggl_map_get(params, GGL_STR("qos"), &val)) {
        if ((val->type != GGL_TYPE_I64) || (val->i64 < 0) || (val->i64 > 2)) {
            GGL_LOGE("Payload received invalid arguments.");
            ggl_return_err(handle, GGL_ERR_INVALID);
            return;
        }
        qos = (uint8_t) val->i64;
    }

    GglError ret = iotcored_register_subscriptions(
        topic_filters, topic_filter_count, handle
    );
    if (ret != GGL_ERR_OK) {
        ggl_return_err(handle, ret);
        return;
    }

    ret = iotcored_mqtt_subscribe(topic_filters, topic_filter_count, qos);
    if (ret != GGL_ERR_OK) {
        iotcored_unregister_subscriptions(handle);
        ggl_return_err(handle, ret);
        return;
    }

    ggl_sub_accept(handle, sub_close_callback, NULL);
}
