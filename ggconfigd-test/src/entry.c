
#include "ggconfigd-test.h"
#include "stdbool.h"
#include <assert.h>
#include <ggl/buffer.h>
#include <ggl/bump_alloc.h>
#include <ggl/core_bus/client.h>
#include <ggl/core_bus/gg_config.h>
#include <ggl/error.h>
#include <ggl/log.h>
#include <ggl/object.h>
#include <ggl/utils.h>
#include <ggl/vector.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#define SUCCESS_STRING "test-and-verify-the-world"
char *command = NULL;
char recipe_dir[PATH_MAX] = { 0 };
char *component_name = "sample";
char *component_version = "1.0.0";

const char *component_name_test = "ggconfigd-test";

GglError run_ggconfigd_test(void) {
    GglKVVec args = GGL_KV_VEC((GglKV[3]) { 0 });

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        GGL_LOGE("ggdeploymentd", "Error getting current working directory.");
        assert(false);
        return GGL_ERR_FAILURE;
    }

    strncat(recipe_dir, cwd, strlen(cwd));
    strncat(
        recipe_dir,
        "/ggconfigd-test/sample-recipe",
        strlen("/ggconfigd-test/sample-recipe")
    );

    GGL_LOGI("ggconfigd-test", "Location of recipe file is %s", recipe_dir);

    GglError ret = ggl_kv_vec_push(
        &args,
        (GglKV) { GGL_STR("recipe_directory_path"),
                  GGL_OBJ(ggl_buffer_from_null_term(recipe_dir)) }
    );
    if (ret != GGL_ERR_OK) {
        assert(false);
        return ret;
    }

    GglKV component;
    if (component_name != NULL) {
        component
            = (GglKV) { ggl_buffer_from_null_term(component_name),
                        GGL_OBJ(ggl_buffer_from_null_term(component_version)) };
        ret = ggl_kv_vec_push(
            &args,
            (GglKV) { GGL_STR("root_component_versions_to_add"),
                      GGL_OBJ((GglMap) { .pairs = &component, .len = 1 }) }
        );
        if (ret != GGL_ERR_OK) {
            assert(false);
            return ret;
        }
    }

    GglBuffer id_mem = GGL_BUF((uint8_t[36]) { 0 });
    GglBumpAlloc alloc = ggl_bump_alloc_init(id_mem);

    GglObject result;
    ret = ggl_call(
        GGL_STR("/aws/ggl/ggdeploymentd"),
        GGL_STR("create_local_deployment"),
        args.map,
        NULL,
        &alloc.alloc,
        &result
    );
    if (ret != GGL_ERR_OK) {
        return ret;
    }

    // Hacky way to wait for deployment. Once we have an API to verify that a
    // given deployment is complete, we should use that.
    ggl_sleep(10);

    // find the version of the active running component
    GglObject result_obj;
    static uint8_t version_resp_mem[10024] = { 0 };
    GglBumpAlloc version_balloc
        = ggl_bump_alloc_init(GGL_BUF(version_resp_mem));

    ret = ggl_gg_config_read(
        GGL_BUF_LIST(
            GGL_STR("services"),
            GGL_STR("com.example.sample"),
            GGL_STR("message")
        ),
        &version_balloc.alloc,
        &result_obj
    );

    if (ret != GGL_ERR_OK) {
        return ret;
    }

    if (result_obj.type != GGL_TYPE_BUF) {
        GGL_LOGE(component_name_test, "Result is not a buffer.");
        return GGL_ERR_FAILURE;
    }

    size_t min = strlen(SUCCESS_STRING);
    if (min > result_obj.buf.len) {
        min = result_obj.buf.len;
    }

    if ((strlen(SUCCESS_STRING) != result_obj.buf.len)
        || (strncmp(SUCCESS_STRING, (const char *) result_obj.buf.data, min)
            != 0)) {
        GGL_LOGE(component_name_test, "Test failed");
        return GGL_ERR_FAILURE;
    }

    return GGL_ERR_OK;
}