#include "Camera.h"
#include <thread>
int main() {
    CameraDevice cd("/dev/video0");
    cd.StartCapturing();
    std::this_thread::sleep_for(std::chrono::seconds(2));
    cd.GetFrame();
    return 0;
}