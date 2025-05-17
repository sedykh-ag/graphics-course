#include <array>
#include <cassert>
#include <optional>
#include <string>

#include "App.hpp"
#include "etna/BlockingTransferHelper.hpp"
#include "etna/DescriptorSet.hpp"
#include "etna/OneShotCmdMgr.hpp"
#include "etna/RenderTargetStates.hpp"

#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>

#include "etna/Sampler.hpp"
#include "spdlog/spdlog.h"
#include "stb_image.h"

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    // GLFW tells us which extensions it needs to present frames to the OS window.
    // Actually rendering anything to a screen is optional in Vulkan, you can
    // alternatively save rendered frames into files, send them over network, etc.
    // Instance extensions do not depend on the actual GPU, only on the OS.
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    // We also need the swapchain device extension to get access to the OS
    // window from inside of Vulkan on the GPU.
    // Device extensions require HW support from the GPU.
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS combination a "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
    etna::initialize(etna::InitParams{
      .applicationName = "Local Shadertoy",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      .physicalDeviceIndexOverride = {},
      .numFramesInFlight = 1,
    });
  }

  // Now we can create an OS window
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this once.
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    // Technically, Vulkan might fail to initialize a swapchain with the requested
    // resolution and pick a different one. This, however, does not occur on platforms
    // we support. Still, it's better to follow the "intended" path.
    resolution = {w, h};
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();


  // TODO: Initialize any additional resources you require here!
  prepareResources();
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    processInput();

    update();

    drawFrame();
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::update()
{
  uniformParams.iResolution = osWindow->getResolution();
  // iMouse is updated in processInput...
  uniformParams.iTime = static_cast<float>(windowing.getTime());

  std::memcpy(uniformBufferObject.data(), &uniformParams, sizeof(UniformParams));
}

void App::processInput()
{
  // keyboard
  if (osWindow->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
    osWindow->askToClose();

  if (osWindow->keyboard[KeyboardKey::kB] == ButtonState::Falling)
    reloadShaders();

  // mouse
  if (osWindow->mouse[MouseButton::mb1] == ButtonState::Rising)  // button just pressed
  {
    uniformParams.iMouse.z = osWindow->mouse.freePos.x;
    uniformParams.iMouse.w = osWindow->mouse.freePos.y; // has "+" sign only on the moment of click
  }

  if (osWindow->mouse[MouseButton::mb1] == ButtonState::High)   // button held
  {
    uniformParams.iMouse.x = osWindow->mouse.freePos.x;
    uniformParams.iMouse.y = osWindow->mouse.freePos.y;
  }

  if (osWindow->mouse[MouseButton::mb1] == ButtonState::Falling) // button just released
  {
    uniformParams.iMouse.z = -glm::abs(uniformParams.iMouse.z);
  }

  if (osWindow->mouse[MouseButton::mb1] != ButtonState::Rising) // button NOT just pressed
  {
    uniformParams.iMouse.w = -glm::abs(uniformParams.iMouse.w);
  }
}

void App::prepareResources()
{
  auto& ctx = etna::get_context();

  etna::create_program("toy_graphics_main",
    {INFLIGHT_FRAMES_SHADERS_ROOT "quad.vert.spv", INFLIGHT_FRAMES_SHADERS_ROOT "main.frag.spv"});

  etna::create_program("toy_graphics_procedural",
    {INFLIGHT_FRAMES_SHADERS_ROOT "quad.vert.spv", INFLIGHT_FRAMES_SHADERS_ROOT "procedural.frag.spv"});

  mainPipeline = ctx.getPipelineManager().createGraphicsPipeline("toy_graphics_main", {
    .fragmentShaderOutput{
      .colorAttachmentFormats = {vk::Format::eB8G8R8A8Srgb}
    }
  });

  proceduralPipeline = ctx.getPipelineManager().createGraphicsPipeline("toy_graphics_procedural", {
    .fragmentShaderOutput{
      .colorAttachmentFormats = {vk::Format::eR8G8B8A8Unorm}
    }
  });

  proceduralImage = ctx.createImage({
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "procedural_image",
    .format = vk::Format::eR8G8B8A8Unorm,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .filter = vk::Filter::eLinear,
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "default_sampler"
  });

  uniformBufferObject = ctx.createBuffer(etna::Buffer::CreateInfo{
    .size = sizeof(UniformParams),
    .bufferUsage = vk::BufferUsageFlagBits::eUniformBuffer,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "uniformBufferObject",
  });

  uniformBufferObject.map();

  // texture
  {
    int width, height, channels;
    stbi_info(TEXTURES_ROOT "wood.png", &width, &height, &channels);

    unsigned char *rawData = stbi_load(TEXTURES_ROOT "wood.png", &height, &width, &channels, STBI_rgb_alpha);
    int imageSize = width * height * 4; // 4 because STBI_rgb_alpha is enforced when loading
    std::span<std::byte const> data(reinterpret_cast<std::byte*>(rawData), imageSize);

    textureMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height))) + 1);

    textureImage = ctx.createImage({
      .extent = vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
      .name = "texture_image",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
      .mipLevels = textureMipLevels
    });

    etna::BlockingTransferHelper transferHelper{{static_cast<uint32_t>(imageSize)}};

    etna::OneShotCmdMgr cmdManager = etna::OneShotCmdMgr{{
      .device = ctx.getDevice(),
      .submitQueue = ctx.getQueue(),
      .queueFamily = ctx.getQueueFamilyIdx()
    }};

    transferHelper.uploadImage(
      cmdManager,
      textureImage,
      0,
      0,
      data
    );

    stbi_image_free(rawData);

    /* generate mip levels */
    vk::CommandBuffer cmdBuf = cmdManager.start();

    ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));

    int mipWidth = width;
    int mipHeight = height;
    // start from 1 cause mip=0 already filled
    for (uint32_t mip = 1; mip < textureMipLevels; mip++)
    {
      vk::ImageBlit blit{
        .srcSubresource = {
          .aspectMask = vk::ImageAspectFlagBits::eColor,
          .mipLevel = mip - 1,
          .baseArrayLayer = 0,
          .layerCount = 1
        },
        .srcOffsets = std::array<vk::Offset3D, 2>{
          vk::Offset3D{0, 0, 0},
          vk::Offset3D{mipWidth, mipHeight, 1}
        },
        .dstSubresource = {
          .aspectMask = vk::ImageAspectFlagBits::eColor,
          .mipLevel = mip,
          .baseArrayLayer = 0,
          .layerCount = 1
        },
        .dstOffsets = std::array<vk::Offset3D, 2>{
          vk::Offset3D{0, 0, 0},
          vk::Offset3D{mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1}
        }
      };

      etna::set_state(
        cmdBuf,
        textureImage.get(),
        vk::PipelineStageFlagBits2::eTransfer,
        vk::AccessFlagBits2::eTransferRead,
        vk::ImageLayout::eGeneral,
        vk::ImageAspectFlagBits::eColor
      );

      etna::flush_barriers(cmdBuf);

      cmdBuf.blitImage(
        textureImage.get(),
        vk::ImageLayout::eGeneral,
        textureImage.get(),
        vk::ImageLayout::eGeneral,
        1,
        &blit,
        vk::Filter::eLinear
      );

      if (mipWidth > 1) mipWidth /= 2;
      if (mipHeight > 1) mipHeight /= 2;
      spdlog::info("Generated mip {} of {}x{}", mip, mipWidth, mipHeight);
    }

    ETNA_CHECK_VK_RESULT(cmdBuf.end());

    cmdManager.submitAndWait(std::move(cmdBuf));

    spdlog::info("Generated {} mip levels", textureMipLevels);
  }

  // skybox texture
  {
    const std::string faces[6] = {"right", "left", "top", "bottom", "front", "back"};

    int width, height, channels;
    stbi_info(TEXTURES_ROOT "skybox/back.jpg", &width, &height, &channels);
    int faceSize = width * height * 4; // 4 because STBI_rgb_alpha is enforced when loading

    skyboxImage = ctx.createImage({
      .extent = vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
      .name = "skybox",
      .format = vk::Format::eR8G8B8A8Srgb,
      .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
      .layers = 6,
      .flags = vk::ImageCreateFlagBits::eCubeCompatible
    });

    etna::BlockingTransferHelper transferHelper{{static_cast<uint32_t>(faceSize)}};

    etna::OneShotCmdMgr cmdManager = etna::OneShotCmdMgr{{
      .device = ctx.getDevice(),
      .submitQueue = ctx.getQueue(),
      .queueFamily = ctx.getQueueFamilyIdx()
    }};

    for (int face = 0; face < 6; face++)
    {
      std::string path = TEXTURES_ROOT "skybox/" + faces[face] + ".jpg";

      {
        int w, h, n;
        stbi_info(path.c_str(), &w, &h, &n);
        assert(w == width && h == height && n == channels); // should be the same for every face
      }

      unsigned char *rawData = stbi_load(path.c_str(), &height, &width, &channels, STBI_rgb_alpha);
      std::span<std::byte const> data(reinterpret_cast<std::byte*>(rawData), faceSize);

      transferHelper.uploadImage(
        cmdManager,
        skyboxImage,
        0,
        face,
        data
      );

      stbi_image_free(rawData);
    }

    spdlog::info("Prepared skybox cubemap.");
  }

  float minLod = static_cast<float>(textureMipLevels / 2); // for testing purposes
  float maxLod = static_cast<float>(textureMipLevels);

  textureSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .filter = vk::Filter::eLinear,
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "texture_sampler",
    .minLod = minLod,
    .maxLod = maxLod
  });

  spdlog::info("Created sampler with .minLod = {} and .maxLod = {}", minLod, maxLod);

  spdlog::info("Prepared resources.");
}

void App::reloadShaders()
{
  const int retval = std::system("cd " GRAPHICS_COURSE_ROOT "/build"
                                  " && cmake --build . --target inflight_frames_shaders");
  if (retval != 0)
    spdlog::warn("Shader recompilation returned a non-zero return code!");
  else
  {
    ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
    etna::reload_shaders();
    spdlog::info("Successfully reloaded shaders!");
  }
}

void App::drawFrame()
{
  // First, get a command buffer to write GPU commands into.
  auto currentCmdBuf = commandManager->acquireNext();

  // Next, tell Etna that we are going to start processing the next frame.
  etna::begin_frame();

  // And now get the image we should be rendering the picture into.
  auto nextSwapchainImage = vkWindow->acquireNext();

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {

      etna::flush_barriers(currentCmdBuf);

      // procedural shader
      {
        auto toyGraphicsProceduralInfo = etna::get_shader_program("toy_graphics_procedural");

        auto set = etna::create_descriptor_set(
          toyGraphicsProceduralInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
            etna::Binding{0, uniformBufferObject.genBinding()}
          });

        etna::RenderTargetState renderTargets(
          currentCmdBuf,
          {{0, 0}, {resolution.x, resolution.y}},
          {{.image = proceduralImage.get(), .view = proceduralImage.getView({})}},
          {});

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, proceduralPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          proceduralPipeline.getVkPipelineLayout(),
          0,
          {set.getVkSet()},
          {});

        etna::set_state(
          currentCmdBuf,
          proceduralImage.get(),
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageAspectFlagBits::eColor
        );

        etna::flush_barriers(currentCmdBuf);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

      // main shader
      {
        auto toyGraphicsMainInfo = etna::get_shader_program("toy_graphics_main");

        auto set = etna::create_descriptor_set(
          toyGraphicsMainInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
            etna::Binding{0, uniformBufferObject.genBinding()},
            etna::Binding{1, proceduralImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{
              2,
              skyboxImage.genBinding(
                defaultSampler.get(),
                vk::ImageLayout::eShaderReadOnlyOptimal,
                etna::Image::ViewParams{0, vk::RemainingMipLevels, 0, 6, std::nullopt, vk::ImageViewType::eCube}
              )},
            etna::Binding{3, textureImage.genBinding(textureSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
          });

        etna::RenderTargetState renderTargets(
          currentCmdBuf,
          {{0, 0}, {resolution.x, resolution.y}},
          {{.image = backbuffer, .view = backbufferView}},
          {});

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, mainPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          mainPipeline.getVkPipelineLayout(),
          0,
          {set.getVkSet()},
          {});

        etna::set_state(
          currentCmdBuf,
          proceduralImage.get(),
          vk::PipelineStageFlagBits2::eFragmentShader,
          vk::AccessFlagBits2::eShaderSampledRead,
          vk::ImageLayout::eShaderReadOnlyOptimal,
          vk::ImageAspectFlagBits::eColor
        );

        etna::set_state(
          currentCmdBuf,
          textureImage.get(),
          vk::PipelineStageFlagBits2::eFragmentShader,
          vk::AccessFlagBits2::eShaderSampledRead,
          vk::ImageLayout::eShaderReadOnlyOptimal,
          vk::ImageAspectFlagBits::eColor
        );

        etna::set_state(
          currentCmdBuf,
          backbuffer,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageAspectFlagBits::eColor
        );

        etna::flush_barriers(currentCmdBuf);

        currentCmdBuf.draw(3, 1, 0, 0);
      }

      // At the end of "rendering", we are required to change how the pixels of the
      // swpchain image are laid out in memory to something that is appropriate
      // for presenting to the window (while preserving the content of the pixels!).
      etna::set_state(
        currentCmdBuf,
        backbuffer,
        // This looks weird, but is correct. Ask about it later.
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        {},
        vk::ImageLayout::ePresentSrcKHR,
        vk::ImageAspectFlagBits::eColor);
      // And of course flush the layout transition.
      etna::flush_barriers(currentCmdBuf);
    }
    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    // Note that the GPU won't start executing our commands before the semaphore is
    // signalled, which will happen when the OS says that the next swapchain image is ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    // Finally, present the backbuffer the screen, but only after the GPU tells the OS
    // that it is done executing the command buffer via the renderingDone semaphore.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to continue rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
