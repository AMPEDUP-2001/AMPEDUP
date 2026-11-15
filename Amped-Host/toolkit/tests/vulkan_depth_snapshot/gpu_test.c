/* Standalone opt-in GPU diagnostic. It extracts the stage-1 comparison from
 * the project's GLSL generator; texture() becomes textureLod() because this
 * harness uses compute, with nearest filtering and a single mip. Thus this
 * exercises the copy, depth view, projection and comparison, not graphics
 * interpolation, implicit LOD, or the backend's cache/lifetime policy. */
#include <vulkan/vulkan.h>
#include <glslang/Include/glslang_c_interface.h>
#include <glslang/Public/resource_limits_c.h>
#include "d3d8_glsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { \
    fprintf(stderr, "%s failed: %d (line %d)\n", #x, (int)r_, __LINE__); return 1; } } while (0)
#define NEED(x) do { if (!(x)) { fprintf(stderr, "failed: %s line %d\n", #x, __LINE__); return 1; } } while (0)

static uint32_t memtype(VkPhysicalDevice p, uint32_t bits, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties m;
    uint32_t i;
    vkGetPhysicalDeviceMemoryProperties(p, &m);
    for (i = 0; i < m.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (m.memoryTypes[i].propertyFlags & flags) == flags) return i;
    return UINT32_MAX;
}

static int shader_code(char *out, size_t cap)
{
    NV2ACombinerState st;
    static char frag[262144];
    char *start, *end, *texture;
    size_t before, len;
    memset(&st, 0, sizeof st);
    st.tex_mode[1] = NV2A_TEXMODE_2D;
    st.amped_shadow_mask = 2;
    st.amped_shadow_func = 1; /* sampled depth LESS projected depth */
    if (d3d8_combiners_generate_glsl(&st, frag, sizeof frag) <= 0) return 0;
    start = strstr(frag, "    {\n        vec2 uv = (i_t1.xy");
    if (!start || !(end = strstr(start, "    }\n"))) return 0;
    end += 6;
    texture = strstr(start, "texture(tex1,");
    if (!texture || texture >= end) return 0;
    before = (size_t)(texture - start);
    len = (size_t)(end - texture);
    return snprintf(out, cap,
        "#version 450\nlayout(local_size_x=1) in;\n"
        "layout(set=0,binding=0) uniform sampler2D tex1;\n"
        "layout(set=0,binding=1,std430) buffer Out { float val[]; } outbuf;\n"
        "layout(push_constant) uniform Push { uint slot; } pushc;\n"
        "struct Pc { vec4 tex_scale[4]; vec4 tex_bias; };\n"
        "void main() { Pc pc; pc.tex_scale[1]=vec4(1); pc.tex_bias=vec4(0);"
        " vec4 i_t1=vec4(0.5,0.5,8388607.5,1); vec4 x9=vec4(0);\n"
        "%.*stextureLod%.*s outbuf.val[pushc.slot]=x9.r; }\n",
        (int)before, start, (int)(len - 7), texture + 7) > 0;
}

static uint32_t *spirv(const char *src, size_t *words)
{
    glslang_input_t in = {0};
    glslang_shader_t *s;
    glslang_program_t *p;
    uint32_t *data = NULL;
    in.language = GLSLANG_SOURCE_GLSL; in.stage = GLSLANG_STAGE_COMPUTE;
    in.client = GLSLANG_CLIENT_VULKAN; in.client_version = GLSLANG_TARGET_VULKAN_1_3;
    in.target_language = GLSLANG_TARGET_SPV; in.target_language_version = GLSLANG_TARGET_SPV_1_5;
    in.code = src; in.default_version = 450; in.default_profile = GLSLANG_NO_PROFILE;
    in.resource = glslang_default_resource();
    if (!(s = glslang_shader_create(&in))) return NULL;
    if (!glslang_shader_preprocess(s, &in) || !glslang_shader_parse(s, &in)) {
        fprintf(stderr, "%s\n", glslang_shader_get_info_log(s)); goto done_shader;
    }
    if (!(p = glslang_program_create())) goto done_shader;
    glslang_program_add_shader(p, s);
    if (!glslang_program_link(p, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        fprintf(stderr, "%s\n", glslang_program_get_info_log(p)); goto done_program;
    }
    glslang_program_SPIRV_generate(p, GLSLANG_STAGE_COMPUTE);
    *words = glslang_program_SPIRV_get_size(p);
    if (*words && (data = malloc(*words * sizeof *data))) glslang_program_SPIRV_get(p, data);
done_program:
    glslang_program_delete(p);
done_shader:
    glslang_shader_delete(s);
    return data;
}

static void transition(VkCommandBuffer cb, VkImage image, VkImageLayout old, VkImageLayout next,
                       VkAccessFlags src, VkAccessFlags dst)
{
    VkImageMemoryBarrier b = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    b.oldLayout = old; b.newLayout = next; b.image = image;
    b.srcAccessMask = src; b.dstAccessMask = dst;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    b.subresourceRange.levelCount = b.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         0, 0, NULL, 0, NULL, 1, &b);
}

int main(int argc, char **argv)
{
    VkInstance inst; VkPhysicalDevice pd[16]; VkDevice dev; VkQueue queue;
    VkPhysicalDeviceProperties device_props; VkFormatProperties props;
    VkQueueFamilyProperties qprops[32]; uint32_t n = 16, qn = 32, q = 0, i, phase;
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkDeviceQueueCreateInfo qi = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    VkDeviceCreateInfo di = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    float priority = 1.0f;
    VkImage images[2]; VkDeviceMemory imem[2]; VkImageView view;
    VkImageCreateInfo ii = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    VkImageViewCreateInfo vi = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    VkMemoryRequirements req; VkMemoryAllocateInfo ai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkBuffer buf; VkDeviceMemory bmem; float *result;
    VkBufferCreateInfo bi = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    VkSampler sampler; VkSamplerCreateInfo sci = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    VkDescriptorSetLayout dsl; VkDescriptorPool pool; VkDescriptorSet ds;
    VkDescriptorSetLayoutBinding bindings[2] = {{0}};
    VkDescriptorSetLayoutCreateInfo dl = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    VkDescriptorPoolSize sizes[2] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};
    VkDescriptorPoolCreateInfo dp = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    VkDescriptorSetAllocateInfo da = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    VkDescriptorImageInfo imageinfo = {0}; VkDescriptorBufferInfo bufferinfo = {0};
    VkWriteDescriptorSet writes[2] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    VkPipelineLayout layout; VkPipeline pipe; VkShaderModule module;
    VkPushConstantRange push = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t) };
    VkPipelineLayoutCreateInfo pl = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VkShaderModuleCreateInfo sm = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    VkComputePipelineCreateInfo cp = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    VkCommandPool cmdpool; VkCommandBuffer cb; VkFence fence;
    VkCommandPoolCreateInfo cpi = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    VkCommandBufferAllocateInfo ca = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    VkCommandBufferBeginInfo cbeg = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VkFenceCreateInfo fi = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkImageSubresourceRange range = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
    VkImageCopy copy = {0}; VkClearDepthStencilValue clear = {0};
    VkMemoryBarrier host = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    static char source[16384]; size_t words = 0; uint32_t *code;
    NEED(shader_code(source, sizeof source)); NEED(glslang_initialize_process());
    code = spirv(source, &words); NEED(code); glslang_finalize_process();
    if (argc > 1 && strcmp(argv[1], "--compile-only") == 0) {
        printf("generated compute diagnostic compiled: %zu SPIR-V words\n", words);
        free(code);
        return 0;
    }
    app.apiVersion = VK_API_VERSION_1_3; ici.pApplicationInfo = &app;
    CK(vkCreateInstance(&ici, NULL, &inst)); CK(vkEnumeratePhysicalDevices(inst, &n, pd)); NEED(n);
    vkGetPhysicalDeviceQueueFamilyProperties(pd[0], &qn, qprops);
    while (q < qn && (qprops[q].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT))
                     != (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) ++q;
    NEED(q < qn); vkGetPhysicalDeviceProperties(pd[0], &device_props);
    printf("Vulkan device: %s\n", device_props.deviceName);
    vkGetPhysicalDeviceFormatProperties(pd[0], VK_FORMAT_D32_SFLOAT_S8_UINT, &props);
    if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)) {
        fprintf(stderr, "SKIP: D32S8 sampled images unsupported\n"); return 77;
    }
    qi.queueFamilyIndex = q; qi.queueCount = 1; qi.pQueuePriorities = &priority;
    di.queueCreateInfoCount = 1; di.pQueueCreateInfos = &qi;
    CK(vkCreateDevice(pd[0], &di, NULL, &dev)); vkGetDeviceQueue(dev, q, 0, &queue);
    ii.imageType = VK_IMAGE_TYPE_2D; ii.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    ii.extent = (VkExtent3D){1,1,1}; ii.mipLevels = ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT; ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    for (i = 0; i < 2; ++i) {
        ii.usage = i ? VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT :
                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        CK(vkCreateImage(dev, &ii, NULL, &images[i]));
        vkGetImageMemoryRequirements(dev, images[i], &req); ai.allocationSize = req.size;
        ai.memoryTypeIndex = memtype(pd[0], req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        NEED(ai.memoryTypeIndex != UINT32_MAX); CK(vkAllocateMemory(dev, &ai, NULL, &imem[i]));
        CK(vkBindImageMemory(dev, images[i], imem[i], 0));
    }
    vi.image = images[1]; vi.viewType = VK_IMAGE_VIEW_TYPE_2D; vi.format = ii.format;
    vi.subresourceRange = range; vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    CK(vkCreateImageView(dev, &vi, NULL, &view));
    bi.size = 3 * sizeof(float); bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    CK(vkCreateBuffer(dev, &bi, NULL, &buf)); vkGetBufferMemoryRequirements(dev, buf, &req);
    ai.allocationSize = req.size; ai.memoryTypeIndex = memtype(pd[0], req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    NEED(ai.memoryTypeIndex != UINT32_MAX); CK(vkAllocateMemory(dev, &ai, NULL, &bmem));
    CK(vkBindBufferMemory(dev, buf, bmem, 0)); CK(vkMapMemory(dev, bmem, 0, bi.size, 0, (void **)&result));
    result[0] = result[1] = result[2] = -1.0f;
    sci.magFilter = sci.minFilter = VK_FILTER_NEAREST; sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = sci.addressModeV = sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    CK(vkCreateSampler(dev, &sci, NULL, &sampler));
    bindings[0].binding = 0; bindings[0].descriptorCount = 1;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    bindings[1].binding = 1; bindings[1].descriptorCount = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    dl.bindingCount = 2; dl.pBindings = bindings; CK(vkCreateDescriptorSetLayout(dev, &dl, NULL, &dsl));
    dp.maxSets = 1; dp.poolSizeCount = 2; dp.pPoolSizes = sizes; CK(vkCreateDescriptorPool(dev, &dp, NULL, &pool));
    da.descriptorPool = pool; da.descriptorSetCount = 1; da.pSetLayouts = &dsl; CK(vkAllocateDescriptorSets(dev, &da, &ds));
    imageinfo.sampler = sampler; imageinfo.imageView = view; imageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    bufferinfo.buffer = buf; bufferinfo.range = bi.size;
    writes[0].dstSet = ds; writes[0].dstBinding = 0; writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; writes[0].pImageInfo = &imageinfo;
    writes[1].dstSet = ds; writes[1].dstBinding = 1; writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; writes[1].pBufferInfo = &bufferinfo;
    vkUpdateDescriptorSets(dev, 2, writes, 0, NULL);
    pl.setLayoutCount = 1; pl.pSetLayouts = &dsl; pl.pushConstantRangeCount = 1; pl.pPushConstantRanges = &push;
    CK(vkCreatePipelineLayout(dev, &pl, NULL, &layout)); sm.codeSize = words * sizeof *code; sm.pCode = code;
    CK(vkCreateShaderModule(dev, &sm, NULL, &module)); free(code);
    cp.layout = layout; cp.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cp.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT; cp.stage.module = module; cp.stage.pName = "main";
    CK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cp, NULL, &pipe));
    cpi.queueFamilyIndex = q; CK(vkCreateCommandPool(dev, &cpi, NULL, &cmdpool));
    ca.commandPool = cmdpool; ca.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ca.commandBufferCount = 1;
    CK(vkAllocateCommandBuffers(dev, &ca, &cb)); CK(vkBeginCommandBuffer(cb, &cbeg));
    transition(cb, images[0], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    transition(cb, images[1], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    copy.srcSubresource.aspectMask = copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    copy.srcSubresource.layerCount = copy.dstSubresource.layerCount = 1; copy.extent = (VkExtent3D){1,1,1};
    for (phase = 0; phase < 3; phase++) {
        if (phase < 2) {
            clear.depth = phase ? 0.75f : 0.25f;
            vkCmdClearDepthStencilImage(cb, images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
            transition(cb, images[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        }
        if (phase != 1) {
            if (phase == 2) transition(cb, images[1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            vkCmdCopyImage(cb, images[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, images[1],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            transition(cb, images[1], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        }
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &ds, 0, NULL);
        vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof phase, &phase);
        vkCmdDispatch(cb, 1, 1, 1);
        if (phase == 0) transition(cb, images[0], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    }
    host.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT; host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT,
                         0, 1, &host, 0, NULL, 0, NULL);
    CK(vkEndCommandBuffer(cb)); CK(vkCreateFence(dev, &fi, NULL, &fence));
    submit.commandBufferCount = 1; submit.pCommandBuffers = &cb; CK(vkQueueSubmit(queue, 1, &submit, fence));
    CK(vkWaitForFences(dev, 1, &fence, VK_TRUE, 10000000000ull));
    printf("depth comparison: initial=%g cached=%g refreshed=%g (expected 1,1,0)\n", result[0],result[1],result[2]);
    i = !(result[0] == 1.0f && result[1] == 1.0f && result[2] == 0.0f);
    vkUnmapMemory(dev, bmem); vkDestroyFence(dev, fence, NULL); vkDestroyCommandPool(dev, cmdpool, NULL);
    vkDestroyPipeline(dev, pipe, NULL); vkDestroyShaderModule(dev, module, NULL);
    vkDestroyPipelineLayout(dev, layout, NULL); vkDestroyDescriptorPool(dev, pool, NULL);
    vkDestroyDescriptorSetLayout(dev, dsl, NULL); vkDestroySampler(dev, sampler, NULL);
    vkDestroyBuffer(dev, buf, NULL); vkFreeMemory(dev, bmem, NULL); vkDestroyImageView(dev, view, NULL);
    for (phase = 0; phase < 2; phase++) { vkDestroyImage(dev, images[phase], NULL); vkFreeMemory(dev, imem[phase], NULL); }
    vkDestroyDevice(dev, NULL); vkDestroyInstance(inst, NULL);
    return (int)i;
}
