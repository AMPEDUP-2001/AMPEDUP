/* Run the opt-in generator checks without a GPU or the Vulkan backend.
 * The emitted GLSL is also compiled by glsl_split where that target exists. */
#include "d3d8_glsl.h"
#include <stdio.h>
#include <string.h>
#ifdef SHADOW_TEST_GLSLANG
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>

static int compile_glsl(const char *source)
{
    glslang_input_t in;
    glslang_shader_t *shader;
    glslang_program_t *program;
    int ok;
    memset(&in, 0, sizeof in);
    in.language = GLSLANG_SOURCE_GLSL;
    in.stage = GLSLANG_STAGE_FRAGMENT;
    in.client = GLSLANG_CLIENT_VULKAN;
    in.client_version = GLSLANG_TARGET_VULKAN_1_3;
    in.target_language = GLSLANG_TARGET_SPV;
    in.target_language_version = GLSLANG_TARGET_SPV_1_5;
    in.default_version = 450;
    in.default_profile = GLSLANG_NO_PROFILE;
    in.messages = GLSLANG_MSG_DEFAULT_BIT;
    in.resource = glslang_default_resource();
    in.code = source;
    shader = glslang_shader_create(&in);
    if (!shader) return 0;
    ok = glslang_shader_preprocess(shader, &in) && glslang_shader_parse(shader, &in);
    if (!ok) fprintf(stderr, "GLSL: %s\n", glslang_shader_get_info_log(shader));
    if (ok) {
        program = glslang_program_create();
        if (!program) ok = 0;
        else {
            glslang_program_add_shader(program, shader);
            ok = glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT);
            if (!ok) fprintf(stderr, "link: %s\n", glslang_program_get_info_log(program));
            if (ok) {
                glslang_program_SPIRV_generate(program, GLSLANG_STAGE_FRAGMENT);
                ok = glslang_program_SPIRV_get_size(program) > 0;
            }
            glslang_program_delete(program);
        }
    }
    glslang_shader_delete(shader);
    return ok;
}
#endif

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    static const char *const ops[8] = { NULL, "<", "==", "<=", ">", "!=", ">=", NULL };
    static char src[256 * 1024], base[256 * 1024], needle[160];
    NV2ACombinerState st;
    int stage, fn, hdr;
    memset(&st, 0, sizeof st);
#ifdef SHADOW_TEST_GLSLANG
    CHECK(glslang_initialize_process());
#endif
    for (stage = 0; stage < 4; stage++) st.tex_mode[stage] = NV2A_TEXMODE_2D;
    for (hdr = 0; hdr < 2; hdr++) {
        st.amped_shadow_mask = 0;
        st.amped_shadow_func = 0;
        CHECK((hdr ? d3d8_combiners_generate_glsl_hdr(&st, base, sizeof base) :
                     d3d8_combiners_generate_glsl(&st, base, sizeof base)) > 0);
        st.amped_shadow_func = 7;
        CHECK((hdr ? d3d8_combiners_generate_glsl_hdr(&st, src, sizeof src) :
                     d3d8_combiners_generate_glsl(&st, src, sizeof src)) > 0);
        CHECK(strcmp(src, base) == 0);
        for (stage = 0; stage < 4; stage++) for (fn = 0; fn < 8; fn++) {
            st.amped_shadow_mask = (uint8_t)(1u << stage);
            st.amped_shadow_func = (uint8_t)fn;
            CHECK((hdr ? d3d8_combiners_generate_glsl_hdr(&st, src, sizeof src) :
                         d3d8_combiners_generate_glsl(&st, src, sizeof src)) > 0);
#ifdef SHADOW_TEST_GLSLANG
            CHECK(compile_glsl(src));
#endif
            snprintf(needle, sizeof needle, "uniform sampler2D tex%d;", stage);
            CHECK(strstr(src, needle));
            if (fn == 0 || fn == 7)
                snprintf(needle, sizeof needle, "x%d = vec4(%s);", 8 + stage, fn ? "1.0" : "0.0");
            else
                snprintf(needle, sizeof needle, "x%d = vec4(sd %s rz ? 1.0 : 0.0);", 8 + stage, ops[fn]);
            CHECK(strstr(src, needle));
            if (fn > 0 && fn < 7) {
                snprintf(needle, sizeof needle, "(i_t%d.xy / i_t%d.w) * pc.tex_scale[%d].xy", stage, stage, stage);
                CHECK(strstr(src, needle));
                snprintf(needle, sizeof needle, "texture(tex%d, uv, pc.tex_bias[%d]).r", stage, stage);
                CHECK(strstr(src, needle));
                snprintf(needle, sizeof needle, "clamp(i_t%d.z / i_t%d.w, 0.0, 16777215.0)", stage, stage);
                CHECK(strstr(src, needle));
                CHECK(strstr(src, "floor(clamp(texture("));
            }
        }
    }
#ifdef SHADOW_TEST_GLSLANG
    glslang_finalize_process();
    puts("64 experimental shadow GLSL variants compiled to SPIR-V; 2 disabled baselines passed");
#else
    puts("64 experimental shadow GLSL variants and 2 disabled baselines passed");
#endif
    return 0;
}
