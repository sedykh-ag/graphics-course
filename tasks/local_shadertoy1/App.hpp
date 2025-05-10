#pragma once

#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>
#include <etna/Sampler.hpp>

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

  etna::Image storageImage;
  etna::Sampler defaultSampler;
  etna::Buffer uniformBufferObject;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;

  UniformParams uniformParams{
    .iTime = 0.0f,
    .iResolution = {0, 0},
    .iMouse = {0.0f, 0.0f, 0.0f, 0.0f},
  };
};
