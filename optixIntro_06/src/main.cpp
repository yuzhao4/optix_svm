/*
 * Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//-----------------------------------------------------------------------------
//
// optixIntro_06
// Shows additionally how to ...
// - use a single closest hit program for all materials.
// - implement BSDF sampling and evaluation functions as bindless callable programs to reduce the OptiX kernel size.
// - implement BSDFs for diffuse reflection (Lambert), specular reflection (tinted mirror) and specular reflection transmission (Glass).
// - implement thin-walled materials.
// - select dynamically which BSDF to use via the material parameters.
// - support nested materials with proper index of refraction handling between media and proper absorption calculation in the integrator.
// - implement different camera projections with bindless callable programs for pinhole, fisheye and sphere projections.
// - show how to switch between the camera projections instantly at runtime.
//
//-----------------------------------------------------------------------------

#include "shaders/app_config.h"

#include "inc/Application.h"

#include <sutil.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

static Application* g_app = nullptr;

static bool displayGUI = true;

static void error_callback(int error, const char* description)
{
  std::cerr << "Error: "<< error << ": " << description << std::endl;
}

void printUsage(const std::string& argv0)
{
  std::cerr << "\nUsage: " << argv0 << " [options]\n";
  std::cerr <<
    "App Options:\n"
    "   ? | help | --help     Print this usage message and exit.\n"
    "  -w | --width <int>     Window client width  (512).\n"
    "  -h | --height <int>    Window client height (512).\n"
    "  -d | --devices <int>   OptiX device selection, each decimal digit selects one device (3210).\n"
    "  -n | --nopbo           Disable OpenGL interop for the image display.\n"
    "  -l | --light           Add an area light to the scene.\n"
    "  -m | --miss  <0|1>     Select the miss shader (0 = black, 1 = white).\n"
    "  -s | --stack <int>     Set the OptiX stack size (1024) (debug feature).\n"
    "  -f | --file <filename> Save image to file and exit.\n"
  "App Keystrokes:\n"
  "  SPACE  Toggles ImGui display.\n"
  "\n"
  << std::endl;
}


int main(int argc, char *argv[])
{
  int  windowWidth  = 1280;//512;
  int  windowHeight = 1024;//512;
  int  devices      = 3210;  // Decimal digits encode OptiX device ordinals. Default 3210 means to use all four first installed devices, when available.
  bool interop      = true;  // Use OpenGL interop Pixel-Bufferobject to display the resulting image. Disable this when running on multi-GPU or TCC driver mode.
  int  stackSize    = 1024;  // Command line parameter just to be able to find the smallest working size.
  bool light        = false; // Add a geometric are light. Best used with miss 0 and 1.
  int  miss         = 1;     // Select the environment light (0 = black, no light; 1 = constant white environment; 3 = spherical environment texture.

  std::string filenameScreenshot;
  bool hasGUI = true;

  // Parse the command line parameters.
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg(argv[i]);

    if (arg == "--help" || arg == "help" || arg == "?")
    {
      printUsage(argv[0]);
      return 0;
    }
    else if (arg == "-w" || arg == "--width")
    {
      if (i == argc - 1)
      {
        std::cerr << "Option '" << arg << "' requires additional argument.\n";
        printUsage(argv[0]);
        return 0;
      }
      windowWidth = atoi(argv[++i]);
    }
    else if (arg == "-h" || arg == "--height")
    {
      if (i == argc - 1)
      {
        std::cerr << "Option '" << arg << "' requires additional argument.\n";
        printUsage(argv[0]);
        return 0;
      }
      windowHeight = atoi(argv[++i]);
    }
    else if (arg == "-d" || arg == "--devices")
    {
      if (i == argc - 1)
      {
        std::cerr << "Option '" << arg << "' requires additional argument.\n";
        printUsage(argv[0]);
        return 0;
      }
      devices = atoi(argv[++i]);
    }
    else if (arg == "-s" || arg == "--stack")
    {
      if (i == argc - 1)
      {
        std::cerr << "Option '" << arg << "' requires additional argument.\n";
        printUsage(argv[0]);
        return 0;
      }
      stackSize = atoi(argv[++i]);
    }
    else if (arg == "-n" || arg == "--nopbo")
    {
      interop = false;
    }
    else if (arg == "-m" || arg == "--miss")
    {
      if (i == argc - 1)
      {
        std::cerr << "Option '" << arg << "' requires additional argument.\n";
        printUsage(argv[0]);
        return 0;
      }
      const int m = atoi(argv[++i]);
      if (0 <= m && m <= 2)
      {
        miss = m;
      }
    }
    else if (arg == "-l" || arg == "--light")
    {
      light = true;
    }
    else if (arg == "-f" || arg == "--file")
    {
      if (i == argc - 1)
      {
        std::cerr << "Option '" << arg << "' requires additional argument.\n";
        printUsage(argv[0]);
        return 0;
      }
      filenameScreenshot = argv[++i];
      hasGUI = false; // Do not render the GUI when just taking a screenshot. (Automated QA feature.)
    }
    else
    {
      std::cerr << "Unknown option '" << arg << "'\n";
      printUsage(argv[0]);
      return 0;
    }
  }

  glfwSetErrorCallback(error_callback);

  if (!glfwInit())
  {
    error_callback(1, "GLFW failed to initialize.");
    return 1;
  }

  GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "optixIntro_06 - Copyright (c) 2018 NVIDIA Corporation", NULL, NULL);
  if (!window)
  {
    error_callback(2, "glfwCreateWindow() failed.");
    glfwTerminate();
    return 2;
  }

  glfwMakeContextCurrent(window);

  if (glewInit() != GL_NO_ERROR)
  {
    error_callback(3, "GLEW failed to initialize.");
    glfwTerminate();
    return 3;
  }

  g_app = new Application(window, windowWidth, windowHeight,
                          devices, stackSize, interop, light, miss);

  if (!g_app->isValid())
  {
    error_callback(4, "Application initialization failed.");
    glfwTerminate();
    return 4;
  }

  // Main loop
  while (!glfwWindowShouldClose(window))
  {
    glfwPollEvents(); // Render continuously.

    //glfwSetWindowSizeLimits(window, 1280, 1024, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    g_app->reshape(windowWidth, windowHeight);
    //std::cout<<"in main window height is: "<<windowHeight<<std::endl;

    if (hasGUI)
    {
      g_app->guiNewFrame();

      //g_app->guiReferenceManual(); // DAR HACK The ImGui "Programming Manual" as example code.

      g_app->guiWindow(); // The OptiX introduction example GUI window.

      g_app->guiEventHandler(); // Currently only reacting on SPACE to toggle the GUI window.

      g_app->render();  // OptiX rendering and OpenGL texture update.
      g_app->display(); // OpenGL display.

      g_app->guiRender(); // Render all ImGUI elements at last.

      glfwSwapBuffers(window);
    }
    else
    {
      for (int i = 0; i < 1024; ++i) // Accumulate 1024 samples per pixel.
      {
        g_app->render();  // OptiX rendering and OpenGL texture update.
      }
      g_app->screenshot(filenameScreenshot);

      glfwSetWindowShouldClose(window, 1);
    }

    //glfwWaitEvents(); // Render only when an event is happening. Needs some glfwPostEmptyEvent() to prevent GUI lagging one frame behind when ending an action.
  }

  // Cleanup
  delete g_app;

  glfwTerminate();

  return 0;
}
