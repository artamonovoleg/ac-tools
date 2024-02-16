#include <list>
#include <stdio.h>
#include <string.h>
#include "compiled/imgui.h"
#include "imgui_impl_ac_renderer.hpp"

#ifdef _MSC_VER
#pragma warning(disable : 4127)
#endif

static constexpr uint32_t MAX_TEXTURES = 1024;

struct ImGui_ImplACH_FrameRenderBuffers {
  ac_buffer vertex_buffer;
  ac_buffer index_buffer;
};

struct ImGui_ImplACH_WindowRenderBuffers {
  uint32_t                          Index;
  uint32_t                          Count;
  ImGui_ImplACH_FrameRenderBuffers* FrameRenderBuffers;
};

struct ImGui_ImplAC_Data {
  ac_imgui_renderer_init_info init_info;
  uint64_t                    buffer_memory_alignment;
  ac_dsl                      dsl;
  ac_descriptor_buffer        db;
  uint32_t                    stack[MAX_TEXTURES];
  int32_t                     stack_size;
  ac_format                   pipeline_format;
  ac_pipeline                 pipelines[2];
  ac_shader                   vertex_shader;
  ac_shader                   pixel_shader;

  struct {
    uint32_t released[MAX_TEXTURES];
    int32_t  released_size;
  } sets[2];
  // Font data
  ac_sampler  font_sampler;
  ac_image    font_image;
  ImTextureID font_set;
  ac_buffer   staging_buffer;

  // Render buffers for main window
  ImGui_ImplACH_WindowRenderBuffers MainWindowRenderBuffers;

  ImGui_ImplAC_Data()
  {
    AC_ZEROP(this);
    buffer_memory_alignment = 256;
  }
};

static ImGui_ImplAC_Data*
ImGui_ImplAC_GetBackendData()
{
  return ImGui::GetCurrentContext()
           ? (ImGui_ImplAC_Data*)ImGui::GetIO().BackendRendererUserData
           : nullptr;
}

static void
check_ac_result(ac_result err)
{
  ImGui_ImplAC_Data* bd = ImGui_ImplAC_GetBackendData();
  if (!bd)
  {
    return;
  }
  ac_imgui_renderer_init_info* v = &bd->init_info;
  if (v->check_ac_result_fn)
  {
    v->check_ac_result_fn(err);
  }
}

static void
CreateOrResizeBuffer(
  ac_buffer&           buffer,
  uint64_t             new_size,
  ac_buffer_usage_bits usage)
{
  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;
  ac_result                    err;
  if (buffer)
  {
    ac_destroy_buffer(buffer);
    buffer = NULL;
  }

  uint64_t vertex_buffer_size_aligned =
    ((new_size - 1) / bd->buffer_memory_alignment + 1) *
    bd->buffer_memory_alignment;

  ac_buffer_info buffer_info = {};
  buffer_info.size = vertex_buffer_size_aligned;
  buffer_info.usage = usage;
  buffer_info.name = "imgui buffer";
  buffer_info.memory_usage = ac_memory_usage_cpu_to_gpu;

  err = ac_create_buffer(v->device, &buffer_info, &buffer);
  check_ac_result(err);
}

static void
ImGui_ImplAC_SetupRenderState(
  ImDrawData*                       draw_data,
  ac_pipeline                       pipeline,
  ac_cmd                            command_buffer,
  ImGui_ImplACH_FrameRenderBuffers* rb,
  int                               fb_width,
  int                               fb_height)
{
  // Bind pipeline:
  {
    ac_cmd_bind_pipeline(command_buffer, pipeline);
  }

  // Bind Vertex And Index Buffer:
  if (draw_data->TotalVtxCount > 0)
  {
    ac_cmd_bind_vertex_buffer(command_buffer, 0, rb->vertex_buffer, 0);
    ac_cmd_bind_index_buffer(
      command_buffer,
      rb->index_buffer,
      0,
      sizeof(ImDrawIdx) == 2 ? ac_index_type_u16 : ac_index_type_u32);
  }

  // Setup viewport:
  {
    ac_cmd_set_viewport(
      command_buffer,
      0,
      0,
      (float)fb_width,
      (float)fb_height,
      0.0f,
      1.0f);
  }

  {
    struct {
      float scale[2];
      float translate[2];
    } pc;

    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

    pc.scale[0] = 2.0f / (R - L);
    pc.scale[1] = 2.0f / (T - B);
    pc.translate[0] = (R + L) / (L - R);
    pc.translate[1] = (T + B) / (B - T);
    ac_cmd_push_constants(command_buffer, sizeof(pc), &pc);
  }
}

static ac_result
ImGui_ImplAC_CreatePipeline(
  ac_device    device,
  uint32_t     samples,
  ac_format    format,
  ac_pipeline* pipeline);

// Render function
void
ac_imgui_renderer_render_draw_data(
  ImDrawData* draw_data,
  ac_format   format,
  ac_cmd      command_buffer)
{
  // Avoid rendering when minimized, scale coordinates for retina displays
  // (screen coordinates != framebuffer coordinates)
  int fb_width =
    (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
  int fb_height =
    (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
  if (fb_width <= 0 || fb_height <= 0)
  {
    return;
  }

  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;

  if (!bd->pipelines[0] || bd->pipeline_format != format)
  {
    if (bd->pipelines[1])
    {
      ac_destroy_pipeline(bd->pipelines[1]);
      bd->pipelines[1] = nullptr;
    }
    bd->pipelines[1] = bd->pipelines[0];
    ac_result res = ImGui_ImplAC_CreatePipeline(
      v->device,
      v->samples,
      format,
      &bd->pipelines[0]);
    if (res != ac_result_success)
    {
      return;
    }

    bd->pipeline_format = format;
  }

  ac_pipeline pipeline = bd->pipelines[0];

  // Allocate array to store enough vertex/index buffers
  ImGui_ImplACH_WindowRenderBuffers* wrb = &bd->MainWindowRenderBuffers;
  if (wrb->FrameRenderBuffers == nullptr)
  {
    wrb->Index = 0;
    wrb->Count = v->frame_count;
    wrb->FrameRenderBuffers = (ImGui_ImplACH_FrameRenderBuffers*)IM_ALLOC(
      sizeof(ImGui_ImplACH_FrameRenderBuffers) * wrb->Count);
    memset(
      wrb->FrameRenderBuffers,
      0,
      sizeof(ImGui_ImplACH_FrameRenderBuffers) * wrb->Count);
  }
  IM_ASSERT(wrb->Count == v->frame_count);
  wrb->Index = (wrb->Index + 1) % wrb->Count;
  ImGui_ImplACH_FrameRenderBuffers* rb = &wrb->FrameRenderBuffers[wrb->Index];

  auto& frame = bd->sets[bd->MainWindowRenderBuffers.Index];
  for (int32_t i = 0; i < frame.released_size; ++i)
  {
    bd->stack[bd->stack_size] = frame.released[i];
    bd->stack_size++;
  }
  frame.released_size = 0;

  if (draw_data->TotalVtxCount > 0)
  {
    // Create or resize the vertex/index buffers
    size_t vertex_size = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    size_t index_size = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (
      rb->vertex_buffer == NULL ||
      ac_buffer_get_size(rb->vertex_buffer) < vertex_size)
    {
      CreateOrResizeBuffer(
        rb->vertex_buffer,
        vertex_size,
        ac_buffer_usage_vertex_bit);
    }
    if (
      rb->index_buffer == NULL ||
      ac_buffer_get_size(rb->index_buffer) < index_size)
    {
      CreateOrResizeBuffer(
        rb->index_buffer,
        index_size,
        ac_buffer_usage_index_bit);
    }

    // Upload vertex/index data into a single contiguous GPU buffer
    ImDrawVert* vtx_dst = nullptr;
    ImDrawIdx*  idx_dst = nullptr;

    ac_result err = ac_buffer_map_memory(rb->vertex_buffer);
    vtx_dst = (ImDrawVert*)ac_buffer_get_mapped_memory(rb->vertex_buffer);
    check_ac_result(err);

    err = ac_buffer_map_memory(rb->index_buffer);
    idx_dst = (ImDrawIdx*)ac_buffer_get_mapped_memory(rb->index_buffer);
    check_ac_result(err);

    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
      const ImDrawList* cmd_list = draw_data->CmdLists[n];
      memcpy(
        vtx_dst,
        cmd_list->VtxBuffer.Data,
        cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
      memcpy(
        idx_dst,
        cmd_list->IdxBuffer.Data,
        cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
      vtx_dst += cmd_list->VtxBuffer.Size;
      idx_dst += cmd_list->IdxBuffer.Size;
    }

    ac_buffer_unmap_memory(rb->vertex_buffer);
    ac_buffer_unmap_memory(rb->index_buffer);
  }

  // Setup desired AC state
  ImGui_ImplAC_SetupRenderState(
    draw_data,
    pipeline,
    command_buffer,
    rb,
    fb_width,
    fb_height);

  // Will project scissor/clipping rectangles into framebuffer space
  ImVec2 clip_off = draw_data->DisplayPos; // (0,0) unless using multi-viewports
  ImVec2 clip_scale =
    draw_data->FramebufferScale; // (1,1) unless using retina display which are
                                 // often (2,2)

  // Render command lists
  // (Because we merged all buffers into a single one, we maintain our own
  // offset into them)
  int global_vtx_offset = 0;
  int global_idx_offset = 0;
  for (int n = 0; n < draw_data->CmdListsCount; n++)
  {
    const ImDrawList* cmd_list = draw_data->CmdLists[n];
    for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
    {
      const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
      if (pcmd->UserCallback != nullptr)
      {
        // User callback, registered via ImDrawList::AddCallback()
        // (ImDrawCallback_ResetRenderState is a special callback value used by
        // the user to request the renderer to reset render state.)
        if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
        {
          ImGui_ImplAC_SetupRenderState(
            draw_data,
            pipeline,
            command_buffer,
            rb,
            fb_width,
            fb_height);
        }
        else
        {
          pcmd->UserCallback(cmd_list, pcmd);
        }
      }
      else
      {
        // Project scissor/clipping rectangles into framebuffer space
        ImVec2 clip_min(
          (pcmd->ClipRect.x - clip_off.x) * clip_scale.x,
          (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
        ImVec2 clip_max(
          (pcmd->ClipRect.z - clip_off.x) * clip_scale.x,
          (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

        // Clamp to viewport as vkCmdSetScissor() won't accept values that are
        // off bounds
        if (clip_min.x < 0.0f)
        {
          clip_min.x = 0.0f;
        }
        if (clip_min.y < 0.0f)
        {
          clip_min.y = 0.0f;
        }
        if (clip_max.x > fb_width)
        {
          clip_max.x = (float)fb_width;
        }
        if (clip_max.y > fb_height)
        {
          clip_max.y = (float)fb_height;
        }
        if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
        {
          continue;
        }

        // Apply scissor/clipping rectangle
        ac_cmd_set_scissor(
          command_buffer,
          (int32_t)clip_min.x,
          (int32_t)clip_min.y,
          (uint32_t)(clip_max.x - clip_min.x),
          (uint32_t)(clip_max.y - clip_min.y));

        // Bind DescriptorSet with font or user texture
        uint32_t desc_set[1] = {(uint32_t)(uintptr_t)pcmd->TextureId};
        if (sizeof(ImTextureID) < sizeof(ImU64))
        {
          // We don't support texture switches if ImTextureID hasn't been
          // redefined to be 64-bit. Do a flaky check that other textures
          // haven't been used.
          IM_ASSERT(pcmd->TextureId == (ImTextureID)(uintptr_t)bd->font_set);
          desc_set[0] = (uint32_t)(uintptr_t)bd->font_set;
        }
        ac_cmd_bind_set(command_buffer, bd->db, ac_space0, 0);
        ac_cmd_bind_set(command_buffer, bd->db, ac_space1, desc_set[0]);

        // Draw
        ac_cmd_draw_indexed(
          command_buffer,
          pcmd->ElemCount,
          1,
          pcmd->IdxOffset + global_idx_offset,
          pcmd->VtxOffset + global_vtx_offset,
          0);
      }
    }
    global_idx_offset += cmd_list->IdxBuffer.Size;
    global_vtx_offset += cmd_list->VtxBuffer.Size;
  }

  // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been
  // called. Our last values will leak into user/application rendering IF:
  // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or
  // VK_DYNAMIC_STATE_SCISSOR dynamic state
  // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself
  // to explicitly set that state. If you use VK_DYNAMIC_STATE_VIEWPORT or
  // VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before
  // rendering. In theory we should aim to backup/restore those values but I am
  // not sure this is possible. We perform a call to vkCmdSetScissor() to set
  // back a full viewport which is likely to fix things for 99% users but
  // technically this is not perfect. (See github #4644)
  ac_cmd_set_scissor(
    command_buffer,
    0,
    0,
    (uint32_t)fb_width,
    (uint32_t)fb_height);
}

ac_result
ac_imgui_renderer_create_font_texture()
{
  ImGuiIO&                     io = ImGui::GetIO();
  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;

  ac_queue queue = ac_device_get_queue(v->device, ac_queue_type_graphics);

  ac_cmd_pool pool;

  ac_cmd_pool_info pool_info = {};
  pool_info.queue = queue;
  AC_RIF(ac_create_cmd_pool(v->device, &pool_info, &pool));

  ac_cmd cmd;
  AC_RIF(ac_create_cmd(pool, &cmd));

  unsigned char* pixels;
  int            width, height;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  size_t upload_size = width * height * 4 * sizeof(char);

  ac_result err;

  // Create the Image:
  {
    ac_image_info info = {};
    info.type = ac_image_type_2d;
    info.format = ac_format_r8g8b8a8_unorm;
    info.width = width;
    info.height = height;
    info.layers = 1;
    info.samples = 1;
    info.levels = 1;
    info.usage = ac_image_usage_srv_bit | ac_image_usage_transfer_dst_bit;

    err = ac_create_image(v->device, &info, &bd->font_image);
    check_ac_result(err);
  }

  // Create the Descriptor Set:
  bd->font_set = ac_imgui_renderer_create_texture(bd->font_image);

  // Create the Upload Buffer:
  {
    ac_buffer_info buffer_info = {};
    buffer_info.size = upload_size;
    buffer_info.usage = ac_buffer_usage_transfer_src_bit;
    buffer_info.memory_usage = ac_memory_usage_cpu_to_gpu;

    err = ac_create_buffer(v->device, &buffer_info, &bd->staging_buffer);

    check_ac_result(err);
  }

  // Upload to Buffer:
  {
    char* map = nullptr;
    err = ac_buffer_map_memory(bd->staging_buffer);
    check_ac_result(err);
    map = (char*)ac_buffer_get_mapped_memory(bd->staging_buffer);
    memcpy(map, pixels, upload_size);
    ac_buffer_unmap_memory(bd->staging_buffer);
  }

  ac_begin_cmd(cmd);

  // Copy to Image:
  {
    ac_image_barrier copy_barrier[1] = {};
    copy_barrier[0].src_stage = ac_pipeline_stage_top_of_pipe_bit;
    copy_barrier[0].dst_stage = ac_pipeline_stage_transfer_bit;
    copy_barrier[0].src_access = ac_access_none;
    copy_barrier[0].dst_access = ac_access_transfer_write_bit;
    copy_barrier[0].old_layout = ac_image_layout_undefined;
    copy_barrier[0].new_layout = ac_image_layout_transfer_dst;
    copy_barrier[0].image = bd->font_image;
    copy_barrier[0].range.layers = 1;
    copy_barrier[0].range.levels = 1;

    ac_cmd_barrier(cmd, 0, NULL, 1, copy_barrier);

    ac_buffer_image_copy region = {};
    region.width = width;
    region.height = height;
    ac_cmd_copy_buffer_to_image(
      cmd,
      bd->staging_buffer,
      bd->font_image,
      &region);

    ac_image_barrier use_barrier[1] = {};
    use_barrier[0].src_stage = ac_pipeline_stage_transfer_bit;
    use_barrier[0].dst_stage = ac_pipeline_stage_pixel_shader_bit;
    use_barrier[0].src_access = ac_access_transfer_write_bit;
    use_barrier[0].dst_access = ac_access_shader_read_bit;
    use_barrier[0].old_layout = ac_image_layout_transfer_dst;
    use_barrier[0].new_layout = ac_image_layout_shader_read;
    use_barrier[0].image = bd->font_image;
    use_barrier[0].range.layers = 1;
    use_barrier[0].range.levels = 1;

    ac_cmd_barrier(cmd, 0, NULL, 1, use_barrier);
  }

  ac_end_cmd(cmd);

  ac_queue_submit_info submit_info = {};
  submit_info.cmd_count = 1;
  submit_info.cmds = &cmd;
  AC_RIF(ac_queue_submit(queue, &submit_info));

  AC_RIF(ac_queue_wait_idle(queue));

  ac_destroy_cmd(cmd);
  ac_destroy_cmd_pool(pool);

  // Store our identifier
  io.Fonts->SetTexID((ImTextureID)(uintptr_t)bd->font_set);

  return ac_result_success;
}

static void
ImGui_ImplAC_CreateShaderModules(ac_device device)
{
  // Create the shader modules
  ImGui_ImplAC_Data* bd = ImGui_ImplAC_GetBackendData();
  if (bd->vertex_shader == NULL)
  {
    ac_shader_info shader_info = {};
    shader_info.stage = ac_shader_stage_vertex;
    shader_info.code = imgui_vs[0];

    ac_result err = ac_create_shader(device, &shader_info, &bd->vertex_shader);
    check_ac_result(err);
  }

  if (bd->pixel_shader == NULL)
  {
    ac_shader_info shader_info = {};
    shader_info.stage = ac_shader_stage_pixel;
    shader_info.code = imgui_fs[0];

    ac_result err = ac_create_shader(device, &shader_info, &bd->pixel_shader);
    check_ac_result(err);
  }
}

static ac_result
ImGui_ImplAC_CreatePipeline(
  ac_device    device,
  uint32_t     samples,
  ac_format    format,
  ac_pipeline* pipeline)
{
  ImGui_ImplAC_Data* bd = ImGui_ImplAC_GetBackendData();

  ac_vertex_layout vl = {};
  vl.binding_count = 1;
  vl.bindings[0].stride = sizeof(ImDrawVert);
  vl.bindings[0].input_rate = ac_input_rate_vertex;
  vl.attribute_count = 3;
  vl.attributes[0].semantic = ac_attribute_semantic_position;
  vl.attributes[0].format = ac_format_r32g32_sfloat;
  vl.attributes[0].offset = IM_OFFSETOF(ImDrawVert, pos);
  vl.attributes[1].semantic = ac_attribute_semantic_texcoord0;
  vl.attributes[1].format = ac_format_r32g32_sfloat;
  vl.attributes[1].offset = IM_OFFSETOF(ImDrawVert, uv);
  vl.attributes[2].semantic = ac_attribute_semantic_color;
  vl.attributes[2].format = ac_format_r8g8b8a8_unorm;
  vl.attributes[2].offset = IM_OFFSETOF(ImDrawVert, col);

  ac_pipeline_info pipe_info = {};
  pipe_info.type = ac_pipeline_type_graphics;

  ac_graphics_pipeline_info* info = &pipe_info.graphics;
  info->vertex_shader = bd->vertex_shader;
  info->pixel_shader = bd->pixel_shader;
  info->dsl = bd->dsl;
  info->vertex_layout = vl;
  info->topology = ac_primitive_topology_triangle_list;
  info->rasterizer_info.polygon_mode = ac_polygon_mode_fill;
  info->rasterizer_info.cull_mode = ac_cull_mode_none;
  info->rasterizer_info.front_face = ac_front_face_counter_clockwise;
  info->samples = (samples != 0) ? samples : 1;

  ac_blend_attachment_state color_attachment[1] = {};

  color_attachment[0].src_factor = ac_blend_factor_src_alpha;
  color_attachment[0].dst_factor = ac_blend_factor_one_minus_src_alpha;
  color_attachment[0].op = ac_blend_op_add;
  color_attachment[0].src_alpha_factor = ac_blend_factor_one;
  color_attachment[0].dst_alpha_factor = ac_blend_factor_one_minus_src_alpha;
  color_attachment[0].alpha_op = ac_blend_op_add;

  info->blend_state_info.attachment_states[0] = color_attachment[0];
  info->color_attachment_count = 1;
  info->color_attachment_formats[0] = format;

  ac_result err = ac_create_pipeline(device, &pipe_info, pipeline);

  check_ac_result(err);

  return err;
}

bool
ImGui_ImplAC_CreateDeviceObjects()
{
  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;
  ac_result                    err;

  ImGui_ImplAC_CreateShaderModules(v->device);

  if (!bd->dsl)
  {
    ac_shader shaders[] = {
      bd->vertex_shader,
      bd->pixel_shader,
    };

    ac_dsl_info dsl_info = {};
    dsl_info.shader_count = AC_COUNTOF(shaders);
    dsl_info.shaders = shaders;

    err = ac_create_dsl(v->device, &dsl_info, &bd->dsl);

    check_ac_result(err);
  }

  if (!bd->db)
  {
    ac_descriptor_buffer_info db_info = {};
    db_info.dsl = bd->dsl;
    db_info.max_sets[0] = 1;
    db_info.max_sets[1] = MAX_TEXTURES;

    err = ac_create_descriptor_buffer(v->device, &db_info, &bd->db);

    check_ac_result(err);
  }

  if (!bd->font_sampler)
  {
    ac_sampler_info info = {};
    info.mag_filter = ac_filter_linear;
    info.min_filter = ac_filter_linear;
    info.mipmap_mode = ac_sampler_mipmap_mode_linear;
    info.address_mode_u = ac_sampler_address_mode_repeat;
    info.address_mode_v = ac_sampler_address_mode_repeat;
    info.address_mode_w = ac_sampler_address_mode_repeat;
    info.min_lod = -1000;
    info.max_lod = 1000;
    err = ac_create_sampler(v->device, &info, &bd->font_sampler);

    check_ac_result(err);

    ac_descriptor descritptor = {};
    descritptor.sampler = bd->font_sampler;
    ac_descriptor_write write = {};
    write.count = 1;
    write.descriptors = &descritptor;
    write.type = ac_descriptor_type_sampler;

    ac_update_set(bd->db, ac_space0, 0, 1, &write);
  }

  return true;
}

void
ac_imgui_renderer_destroy_font_upload_objects()
{
  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;

  ac_destroy_buffer(bd->staging_buffer);
  bd->staging_buffer = NULL;
}

void
ImGui_ImplAC_DestroyDeviceObjects()
{
  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;

  ImGui_ImplACH_WindowRenderBuffers* wrb = &bd->MainWindowRenderBuffers;

  for (uint32_t i = 0; i < wrb->Count; ++i)
  {
    ac_destroy_buffer(wrb->FrameRenderBuffers[i].index_buffer);
    ac_destroy_buffer(wrb->FrameRenderBuffers[i].vertex_buffer);
  }
  IM_FREE(wrb->FrameRenderBuffers);
  ac_imgui_renderer_destroy_font_upload_objects();
  ac_destroy_buffer(bd->staging_buffer);
  bd->staging_buffer = NULL;

  if (bd->pipelines[0])
  {
    ac_destroy_pipeline(bd->pipelines[0]);
  }
  if (bd->pipelines[1])
  {
    ac_destroy_pipeline(bd->pipelines[1]);
  }
  ac_destroy_descriptor_buffer(bd->db);
  ac_destroy_dsl(bd->dsl);

  ac_destroy_shader(bd->vertex_shader);
  ac_destroy_shader(bd->pixel_shader);
  ac_destroy_image(bd->font_image);
  ac_destroy_sampler(bd->font_sampler);
}

ac_result
ac_imgui_renderer_init(const ac_imgui_renderer_init_info* info)
{
  ImGuiIO& io = ImGui::GetIO();
  IM_ASSERT(
    io.BackendRendererUserData == nullptr &&
    "Already initialized a renderer backend!");

  // Setup backend capabilities flags
  ImGui_ImplAC_Data* bd = IM_NEW(ImGui_ImplAC_Data)();
  io.BackendRendererUserData = (void*)bd;
  io.BackendRendererName = "imgui_impl_vulkan";
  io.BackendFlags |=
    ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the
                                            // ImDrawCmd::VtxOffset field,
                                            // allowing for large meshes.

  IM_ASSERT(info->device);
  IM_ASSERT(
    info->frame_count > 0 && info->frame_count <= AC_MAX_FRAME_IN_FLIGHT);

  bd->init_info = *info;

  ImGui_ImplAC_CreateDeviceObjects();

  for (uint32_t j = 0; j < MAX_TEXTURES; ++j)
  {
    bd->stack[j] = j;
  }
  bd->stack_size = MAX_TEXTURES;

  return ac_result_success;
}

void
ac_imgui_renderer_shutdown(void)
{
  ImGui_ImplAC_Data* bd = ImGui_ImplAC_GetBackendData();
  IM_ASSERT(
    bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
  ImGuiIO& io = ImGui::GetIO();

  ImGui_ImplAC_DestroyDeviceObjects();
  io.BackendRendererName = nullptr;
  io.BackendRendererUserData = nullptr;
  io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
  IM_DELETE(bd);
}

void
ac_imgui_renderer_new_frame(void)
{
  ImGui_ImplAC_Data* bd = ImGui_ImplAC_GetBackendData();
  IM_ASSERT(bd != nullptr && "Did you call ImGui_ImplAC_Init()?");
  IM_UNUSED(bd);
}

IMGUI_IMPL_API ImTextureID
ac_imgui_renderer_create_texture(ac_image image)
{
  ImGui_ImplAC_Data*           bd = ImGui_ImplAC_GetBackendData();
  ac_imgui_renderer_init_info* v = &bd->init_info;

  if (!bd->stack_size)
  {
    return nullptr;
  }

  bd->stack_size--;
  uint32_t set = bd->stack[bd->stack_size];

  ac_descriptor descriptor = {};
  descriptor.image = image;

  ac_descriptor_write write = {};
  write.count = 1;
  write.descriptors = &descriptor;
  write.type = ac_descriptor_type_srv_image;

  ac_update_set(bd->db, ac_space1, set, 1, &write);

  return (ImTextureID)(uintptr_t)set;
}

IMGUI_IMPL_API void
ac_imgui_renderer_destroy_texture(ImTextureID texture)
{
  if (!texture)
  {
    return;
  }

  ImGui_ImplAC_Data* bd = ImGui_ImplAC_GetBackendData();

  auto& frame = bd->sets[bd->MainWindowRenderBuffers.Index];

  frame.released[frame.released_size] = (uint32_t)(uintptr_t)texture;
  frame.released_size++;
}
