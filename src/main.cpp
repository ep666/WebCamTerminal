#include "Camera.h"
#include <thread>
int main() {
    CameraDevice cd("/dev/video0");
    cd.StartCapturing();
    cd.GetFrame();
    return 0;
}