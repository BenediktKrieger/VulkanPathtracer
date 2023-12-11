#pragma once

#include <Core.h>

namespace vk {
    class GUI
    {
    private:
        vk::Core* _core{nullptr};
        SDL_Window* _window{nullptr};
        vk::RenderPass _renderPass;
        vk::DescriptorPool _pool;
        std::vector<vk::Framebuffer> _framebuffers;
    public:
        GUI();
        GUI(vk::Core* core);
        void initRenderPass();
        void initFrambuffers();
        void render(vk::CommandBuffer &cmd, uint32_t index);
        void update();
        void additionalWindows();
        void handleInput(const SDL_Event *event);
        void destroyFramebuffer();
        void destroy();
        ~GUI();
    };
}
