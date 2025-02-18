#include "Camera.h"
#include "FrameConverter.h"

#include <thread>


void getInput(bool* x) {
    char input;
    while(1) {
        std::cin >> input;

        if (input == 'q') {
            *x = false;
            return;
        }
    }
}


int main() {
    CameraDevice cd("/dev/video0", CameraDevice::IOMethod::IO_METHOD_MMAP);
    cd.StartCapturing();
    const int width = cd.GetFormat().Width;
    const int height = cd.GetFormat().Height;
    int frameCounter = 0;
    bool runing = true;
    std::thread t(getInput, &runing);
    t.detach();

    while(runing) {
        const auto& frame = cd.GetFrame();
        system("clear");
        FrameConverter::ConvertAndPrint(frame, height, width);
        ++frameCounter;
        std::cout << "\n\n\n=============FrameNo: " << frameCounter << "=============\n\n\n";
    }

    system("clear");
    return 0;
}