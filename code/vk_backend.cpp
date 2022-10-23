// #define VMA_IMPLEMENTATION
#define _VMA_PUBLIC_INTERFACE

#if !defined(CGLTF_IMPLEMENTATION)
#define CGLTF_IMPLEMENTATION
#endif

#if !defined(STB_IMAGE_IMPLEMENTATION)
#define STB_IMAGE_IMPLEMENTATION
#endif

#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "vk_backend.h"

#if defined(VULKAN) && !defined(GL)


#include "utils.h"
#include "vk_Mesh.h"
#include "Camera.h"

Camera camera {};

struct Entity
{
    SkinnedModel model;
    Transform    _transform;

    int32_t draw_data_idx;
};

std::vector<Entity> entities {};


extern int main(int argc, char **argv)
{
    VulkanInit();

    camera.CameraCreate({ 0, 20, 44 }, 45.f, WIDTH / (float)HEIGHT, .8f, 4000.f);
    camera._pitch  = -20;
    gActive_camera = &camera;


    entities.resize(2);
    for (size_t i = 0; i < entities.size(); i++) {
        static float z = 0;
        static float x = 0;

        int   distanceFactor      = 22;
        int   max_entities_on_row = 6;
        float startingOffset      = 0.f;

        if (i > 0) x++;
        if (x == max_entities_on_row) {
            x = 0;
            z++;
        }


        if (i == 0) {
            entities[i].model.Create("assets/warrior/warrior.gltf"); // fixme: if for some reason this fails to load
            // then the following line will crash
            entities[i]._transform.translation = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation    = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale       = glm::vec3(.10f);
            entities[i].draw_data_idx          = i;

            entities[i].model._current_animation     = &entities[i].model._animations[0];
            entities[i].model._should_play_animation = true;
        } else if (i == 1) {
            entities[i].model.Create("assets/terrain/terrain.gltf"); // fixme: if for some reason this fails to load
            // then the following line will crash
            entities[i]._transform.translation                     = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation                        = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale                           = glm::vec3(1000.0f);
            entities[i].draw_data_idx                              = i;
            entities[i].model._meshes[0].material_data[0].tiling_x = 10;
            entities[i].model._meshes[0].material_data[0].tiling_y = 10;

        } else {
            // entities[i].model.Create("assets/warrior/warrior.gltf"); // fixme: if for some reason this fails to load
            // then the following line will crash
            entities[i].model._meshes                  = entities[0].model._meshes;
            entities[i].model._mesh_data               = entities[0].model._mesh_data;
            entities[i].model.vertex_buffer            = entities[0].model.vertex_buffer;
            entities[i].model.vertex_buffer_allocation = entities[0].model.vertex_buffer_allocation;

            entities[i]._transform.translation       = { startingOffset + x * distanceFactor, 0.f, startingOffset + z * distanceFactor };
            entities[i]._transform.rotation          = glm::quat({ 0, 0, 0 });
            entities[i]._transform.scale             = glm::vec3(10.0f);
            entities[i].draw_data_idx                = i;
            entities[i].model._current_animation     = &entities[i].model._animations[0];
            entities[i].model._should_play_animation = true;
        }
    }


    bool     bQuit            = false;
    uint32_t frame_in_flight  = 0;
    bool     window_minimized = false;

    uint64_t lastCycleCount = __rdtsc();
    uint64_t lastCounter    = SDL_GetPerformanceCounter();
    uint64_t perfFrequency  = SDL_GetPerformanceFrequency();
    float    dt             = 0;


    //
    // Main loop
    //
    SDL_Event e;
    while (!bQuit) {
        //
        // Input handling
        //
        {
            while (SDL_PollEvent(&e) != 0) {
                SDL_Keycode key = e.key.keysym.sym;

                switch (e.type) {
                    case SDL_KEYUP:
                        if (key == SDLK_ESCAPE) bQuit = true;

                        if (key == SDLK_UP || key == SDLK_w) input.up = false;
                        if (key == SDLK_DOWN || key == SDLK_s) input.down = false;
                        if (key == SDLK_LEFT || key == SDLK_a) input.left = false;
                        if (key == SDLK_RIGHT || key == SDLK_d) input.right = false;
                        if (key == SDLK_q) input.Q = false;
                        if (key == SDLK_e) input.E = false;
                        break;

                    case SDL_KEYDOWN:
                        if (key == SDLK_UP || key == SDLK_w) input.up = true;
                        if (key == SDLK_DOWN || key == SDLK_s) input.down = true;
                        if (key == SDLK_LEFT || key == SDLK_a) input.left = true;
                        if (key == SDLK_RIGHT || key == SDLK_d) input.right = true;
                        if (key == SDLK_q) input.Q = true;
                        if (key == SDLK_e) input.E = true;
                        if (key == SDLK_f) SDL_SetWindowFullscreen(gWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
                        break;
                        // END Key Events

                    case SDL_MOUSEBUTTONDOWN:
                        if (e.button.button == 1) input.mouse.left = true;
                        if (e.button.button == 3) input.mouse.right = true;
                        break;

                    case SDL_MOUSEBUTTONUP:
                        if (e.button.button == 1) input.mouse.left = false;
                        if (e.button.button == 3) input.mouse.right = false;
                        break;

                    case SDL_MOUSEMOTION:
                        input.mouse.xrel = e.motion.xrel;
                        input.mouse.yrel = e.motion.yrel;
                        break;
                        ///////////////////////////////

                    case SDL_QUIT:
                        bQuit = true;

                    // this case that embeds another switch is placed at the bottom of the outer switch
                    // because some reason the case that follows this one is always triggered on first few runs
                    case SDL_WINDOWEVENT:
                        switch (e.window.event) {
                            case SDL_WINDOWEVENT_MINIMIZED:
                                window_minimized = true;
                                break;
                            case SDL_WINDOWEVENT_RESTORED:
                                if (window_minimized) window_minimized = false;
                                break;

                                // SDL_QUIT happens first
                                // case SDL_WINDOWEVENT_CLOSE:
                                //     bQuit = true;
                                //     break;

                            default:
                                break;
                        } // e.window.event
                } // e.type
            } // e
        }
        if (window_minimized) continue;


        gActive_camera->CameraUpdate(&input, dt);
        gActive_camera->_aspect = WIDTH / (float)HEIGHT;


        VkResult result;

        VkCommandBuffer graphics_cmd_buffer = gFrames[frame_in_flight].graphics_command_buffer;

        {
            VKCHECK(vkWaitForFences(gDevice, 1, &gFrames[frame_in_flight].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice, 1, &gFrames[frame_in_flight].render_fence));
            VKCHECK(result = vkAcquireNextImageKHR(gDevice, gSwapchain._handle, SECONDS(1), 0, gFrames[frame_in_flight].render_fence, &gFrames[frame_in_flight].image_idx));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gSwapchain.Create(gDevice, gPhysical_device, gSurface, gSwapchain._handle);
            }
        }

        {
            VKCHECK(vkResetCommandBuffer(graphics_cmd_buffer, 0));

            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            VKCHECK(vkBeginCommandBuffer(graphics_cmd_buffer, &begin_info));





            //
            // Swapchain image layout transition for Rendering
            //
            TransitionImageLayout(graphics_cmd_buffer, gSwapchain._images[gFrames[frame_in_flight].image_idx],
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, false);



            {
                VkClearValue clear_values[2] = {};
                clear_values[0].color        = { .5f, .3f, .5f, 1.f };

                VkRenderingAttachmentInfo color_attachment_info = {};
                color_attachment_info.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                color_attachment_info.imageView                 = gSwapchain._image_views[gFrames[frame_in_flight].image_idx];
                color_attachment_info.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
                color_attachment_info.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
                color_attachment_info.storeOp                   = VK_ATTACHMENT_STORE_OP_STORE;
                color_attachment_info.clearValue                = clear_values[0];

                color_attachment_info.resolveImageLayout;
                color_attachment_info.resolveImageView; // https://vulkan-tutorial.com/Multisampling
                color_attachment_info.resolveMode;

                clear_values[1].depthStencil.depth = { 1.f };

                VkRenderingAttachmentInfo depth_attachment_info = {};
                depth_attachment_info.sType                     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
                depth_attachment_info.imageView                 = gSwapchain._depth_image_view;
                depth_attachment_info.imageLayout               = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
                depth_attachment_info.loadOp                    = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depth_attachment_info.storeOp                   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depth_attachment_info.clearValue                = clear_values[1];

                VkRenderingInfo rendering_info      = {};
                rendering_info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
                rendering_info.renderArea.extent    = { WIDTH, HEIGHT };
                rendering_info.layerCount           = 1;
                rendering_info.colorAttachmentCount = 1;
                rendering_info.pColorAttachments    = &color_attachment_info;
                rendering_info.pDepthAttachment     = &depth_attachment_info;
                rendering_info.pStencilAttachment; // todo
                vkCmdBeginRendering(graphics_cmd_buffer, &rendering_info);





                ////////////////////////////////////////////
                // Rendering
                //
                {
                    //
                    //  Bindings
                    //
                    vkCmdBindPipeline(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline);

                    vkCmdBindDescriptorSets(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 0, 1, &gView_projection_sets[frame_in_flight], 0, NULL);
                    vkCmdBindDescriptorSets(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 1, 1, &gDraw_data_sets[frame_in_flight], 0, NULL);
                    vkCmdBindDescriptorSets(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 2, 1, &gMaterial_data_sets[frame_in_flight], 0, NULL);
                    vkCmdBindDescriptorSets(graphics_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gDefault_graphics_pipeline_layout, 3, 1, &gBindless_textures_set, 0, NULL);

                    ((GlobalUniforms *)mapped_view_proj_ptrs[frame_in_flight])->projection = gActive_camera->_projection;
                    ((GlobalUniforms *)mapped_view_proj_ptrs[frame_in_flight])->view       = gActive_camera->_view;
                    ((GlobalUniforms *)mapped_view_proj_ptrs[frame_in_flight])->projection[1][1] *= -1;

                    VkViewport viewport {};
                    viewport.minDepth = 0;
                    viewport.maxDepth = 1;
                    viewport.width    = (float)WIDTH;
                    viewport.height   = (float)HEIGHT;

                    VkRect2D scissor {};
                    scissor.extent.width  = WIDTH;
                    scissor.extent.height = HEIGHT;

                    vkCmdSetViewport(graphics_cmd_buffer, 0, 1, &viewport);
                    vkCmdSetScissor(graphics_cmd_buffer, 0, 1, &scissor);


                    for (size_t i = 0; i < entities.size(); i++) {


                        glm::mat4 model = glm::mat4(1);

                        model = glm::translate(model, entities[i]._transform.translation)
                            * glm::rotate(model, (glm::radians(entities[i]._transform.rotation.z)), glm::vec3(0, 0, 1))
                            * glm::rotate(model, (glm::radians(entities[i]._transform.rotation.y)), glm::vec3(0, 1, 0))
                            * glm::rotate(model, (glm::radians(entities[i]._transform.rotation.x)), glm::vec3(1, 0, 0))
                            * glm::scale(model, glm::vec3(entities[i]._transform.scale));

                        ((ObjectData *)mapped_draw_data_ptrs[frame_in_flight])[entities[i].draw_data_idx];

                        ((ObjectData *)mapped_draw_data_ptrs[frame_in_flight])[entities[i].draw_data_idx].model = model;

                        PushConstants constants;
                        constants.draw_data_idx = entities[i].draw_data_idx;

                        if (entities[i].model._current_animation) {
                            entities[i].model.AnimationUpdate(dt);

                            constants.has_joints = 1;
                            auto joint_matrices  = entities[i].model._current_animation->joint_matrices;
                            for (size_t mat_idx = 0; mat_idx < joint_matrices.size(); mat_idx++) {
                                ((ObjectData *)mapped_draw_data_ptrs[frame_in_flight])[entities[i].draw_data_idx].joint_matrices[mat_idx] = joint_matrices[mat_idx];
                            }
                        }


                        vkCmdPushConstants(graphics_cmd_buffer, gDefault_graphics_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &constants);

                        entities[i].model.Draw(graphics_cmd_buffer, frame_in_flight);
                    }
                }
                //////////////////////////////////////////////


                vkCmdEndRendering(graphics_cmd_buffer);
            }



            //
            // Swapchain image layout transition for presenting
            //
            {
                VkImageMemoryBarrier2 image_memory_barrier_before_presenting = {};
                image_memory_barrier_before_presenting.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

                image_memory_barrier_before_presenting.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                image_memory_barrier_before_presenting.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                image_memory_barrier_before_presenting.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                // image_memory_barrier_before_presenting.dstAccessMask         = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;

                image_memory_barrier_before_presenting.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                image_memory_barrier_before_presenting.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

                image_memory_barrier_before_presenting.image                       = gSwapchain._images[gFrames[frame_in_flight].image_idx];
                image_memory_barrier_before_presenting.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_memory_barrier_before_presenting.subresourceRange.layerCount = 1;
                image_memory_barrier_before_presenting.subresourceRange.levelCount = 1;

                VkDependencyInfo dependency_info        = {};
                dependency_info.sType                   = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependency_info.imageMemoryBarrierCount = 1;
                dependency_info.pImageMemoryBarriers    = &image_memory_barrier_before_presenting;
                vkCmdPipelineBarrier2(graphics_cmd_buffer, &dependency_info);
            }




            VKCHECK(vkEndCommandBuffer(graphics_cmd_buffer));
        }



        //
        // Command Buffer submission
        //

        {
            VkCommandBufferSubmitInfo command_buffer_submit_info = {};
            command_buffer_submit_info.sType                     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
            command_buffer_submit_info.pNext                     = NULL;
            command_buffer_submit_info.commandBuffer             = graphics_cmd_buffer;
            command_buffer_submit_info.deviceMask;

            VkSemaphoreSubmitInfo wait_semephore_info = {};
            wait_semephore_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            wait_semephore_info.semaphore             = gFrames[frame_in_flight].present_semaphore;
            wait_semephore_info.value;
            wait_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            wait_semephore_info.deviceIndex;

            VkSemaphoreSubmitInfo signal_semephore_info = {};
            signal_semephore_info.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            signal_semephore_info.semaphore             = gFrames[frame_in_flight].render_semaphore;
            signal_semephore_info.value;
            signal_semephore_info.stageMask = VK_PIPELINE_STAGE_NONE;
            signal_semephore_info.deviceIndex;

            VkSubmitInfo2 submit_info = {};
            submit_info.sType         = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
            submit_info.flags;
            submit_info.waitSemaphoreInfoCount = 0;
            // submit_info.pWaitSemaphoreInfos      = &wait_semephore_info; // wait for image to be presented before rendering again
            submit_info.commandBufferInfoCount   = 1;
            submit_info.pCommandBufferInfos      = &command_buffer_submit_info;
            submit_info.signalSemaphoreInfoCount = 1;
            submit_info.pSignalSemaphoreInfos    = &signal_semephore_info;

            VKCHECK(vkWaitForFences(gDevice, 1, &gFrames[frame_in_flight].render_fence, VK_TRUE, SECONDS(1)));
            VKCHECK(vkResetFences(gDevice, 1, &gFrames[frame_in_flight].render_fence));

            VKCHECK(vkQueueSubmit2(gQueues._graphics, 1, &submit_info, gFrames[frame_in_flight].render_fence));
        }


        {
            //
            // Presenting
            //
            VkPresentInfoKHR present_info   = {};
            present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            present_info.waitSemaphoreCount = 1;
            present_info.pWaitSemaphores    = &gFrames[frame_in_flight].render_semaphore; // the semaphore to wait upon before presenting
            present_info.swapchainCount     = 1;
            present_info.pSwapchains        = &gSwapchain._handle;
            present_info.pImageIndices      = &gFrames[frame_in_flight].image_idx;
            present_info.pResults;

            VKCHECK(result = vkQueuePresentKHR(gQueues._present, &present_info));
            if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                gSwapchain.Create(gDevice, gPhysical_device, gSurface, gSwapchain._handle);
            }
        }

        ++frame_in_flight;
        frame_in_flight = frame_in_flight % gSwapchain._image_count;





        uint64_t endCounter = SDL_GetPerformanceCounter();
        uint64_t frameDelta = endCounter - lastCounter;

        dt = frameDelta / (float)perfFrequency;


        uint64_t endCycleCount   = __rdtsc();
        uint64_t cycleDelta      = endCycleCount - lastCycleCount;
        float    MCyclesPerFrame = (float)(cycleDelta / (float)(1000 * 1000));
        lastCounter              = endCounter;
        lastCycleCount           = endCycleCount;



        static float acc = 0;
        if ((acc += dt) > 1) {
            static char buffer[256];
            sprintf_s(buffer, sizeof(buffer), "%.2f fps | %.2f ms | %.2f MCycles | CPU: %.2f Mhz",
                1 / dt,
                dt * 1000,
                MCyclesPerFrame,
                MCyclesPerFrame / dt);

            SDL_SetWindowTitle(gWindow, buffer);
            acc = 0;
        }
    }
    return 0;

    vkDestroySurfaceKHR(gInstance, gSurface, NULL);
    vkDestroyInstance(gInstance, NULL);
}
#endif