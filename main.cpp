#include <iostream>
#include <thread>
#include "particlefire.h"

int main() {
    Util::Renderer::GLFWUtil glfwUtil;
    particlefire renderer;
    static bool quitKeyPressed = false;

    renderer.wantOpenXR = true;
    renderer.useLegacyOpenXR = true;

    //test creating the openxr init.  if this fails, then allow going back to glfw local window
    // to properly fix, so the loader can find it, ensure the json can be found in the RUNTIME
    // environment variable: XR_RUNTIME_JSON
    if(renderer.wantOpenXR) {
        renderer.createOpenXR();
        renderer.createOpenXRSystem(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY);
    }

    if(!renderer.wantOpenXR) {
        glfwUtil.window_init(1280, 1024, &renderer);
        renderer.width = glfwUtil.getWidth();
        renderer.height = glfwUtil.getHeight();
        renderer.setGLFWUtil(&glfwUtil);
    } else {
        // Spawn a thread to wait for a keypress
        auto exitPollingThread = std::thread{[] {
            printf("Press any key to shutdown...\n");
            (void)getchar();
            quitKeyPressed = true;
        }};
        exitPollingThread.detach();
    }
    renderer.createVulkan();
    if(!renderer.wantOpenXR)
        renderer.setSurface(glfwUtil.setSurface(renderer.getInstance()));
    renderer.finalizeSetup();

    int frameCount = 0;
    std::vector<float> fps;
    while (!quitKeyPressed) {
        if(!renderer.wantOpenXR) quitKeyPressed = glfwUtil.isStillRunning();
        auto frame_begin = std::chrono::high_resolution_clock::now();
        glfwPollEvents();
        renderer.GiveTime(frame_begin);
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto skew_end = std::chrono::high_resolution_clock::now();
        auto skewAmount = skew_end - frame_end;
        auto frameAmount = frame_end - frame_begin;
        frameAmount -= skewAmount;
        long sleepMS = static_cast<long>(1.0f / 60.0f * 1000.0f);
        auto timeTakenMS = std::chrono::duration_cast<std::chrono::milliseconds>(frameAmount);
        sleepMS -= (timeTakenMS.count() > sleepMS)?0:timeTakenMS.count();
        std::this_thread::sleep_for(std::chrono::milliseconds (sleepMS));
        frame_end = std::chrono::high_resolution_clock::now();
        fps.push_back(1000.0f / std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_begin).count());
        if (renderer.displayFPS && (frameCount++ % 10) == 0) {
            float avgFPS = 0;
            for(auto cFPS : fps) {
                avgFPS += cFPS;
            }
            renderer.lastFPS = avgFPS / static_cast<float>(fps.size());
            if(!renderer.settings.overlay)
                printf("FPS is %.1f\n", avgFPS / static_cast<float>(fps.size()));
            fps.clear();
        }
    }
    return 0;
}
