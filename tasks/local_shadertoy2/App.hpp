#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

#include "etna/GraphicsPipeline.hpp"
#include "wsi/OsWindowingManager.hpp"

#include "shaders/UniformParams.h"


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();
  void prepareResources();
  void processInput();
  void reloadShaders();
  void update();

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

  etna::ComputePipeline computePipeline;
  etna::GraphicsPipeline mainPipeline;
  etna::GraphicsPipeline proceduralPipeline;

  etna::Image proceduralImage;

  etna::Sampler defaultSampler;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  etna::Buffer uniformBufferObject;
  UniformParams uniformParams{
    .iTime = 0.0f,
    .iResolution = {0, 0},
    .iMouse = {0.0f, 0.0f, 0.0f, 0.0f},
  };
};
