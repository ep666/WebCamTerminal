#include "Camera.h"
#include "FrameConverter.h"
#include "Params.h"

#include <thread>

#include <termios.h>
#include <unistd.h>

void GetInput(bool* x) {
    char input;
    struct termios oldSet, newSet;
    tcgetattr(fileno(stdin), &oldSet);
    newSet = oldSet;
    newSet.c_lflag &= (~ICANON & ~ECHO);
    tcsetattr(fileno(stdin), TCSANOW, &newSet);

    while(1) {
        fd_set set;
        struct timeval tv;

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&set);
        FD_SET(fileno(stdin), &set);

        int res = select(fileno(stdin) + 1, &set, 0, 0, &tv);

        if (res > 0) {
            read(fileno(stdin), &input, 1);
            if (input == 'q') {
                break;
            }
        } else if (res < 0) {
            throw std::runtime_error("Input failed");
        } else {
            continue;
        }
    }

    tcsetattr(fileno(stdin), TCSANOW, &oldSet);
    *x = false;
}


void MainLoop(CameraDevice& cd, const int height, const int width,
                     bool& isRuning, const std::string& grayScale) {
    while(isRuning) {
        const auto& frame = cd.GetFrame();
        system("clear");
        FrameConverter::ConvertAndPrint(frame, height, width, grayScale);
        std::cout << "\n=====Press q to exit=====\n";
    }
}


int main() {
    CameraParams params;

    Configure(params);

    CameraDevice cd(params.DevicePath, params.FormatInfo,
                    params.BufferSize, params.IoMethod);

    cd.StartCapturing();

    bool runing = true;
    std::thread t(GetInput, &runing);
    t.detach();

    MainLoop(cd, cd.GetFormat().Height, cd.GetFormat().Width, runing, params.GrayScaleDepth);

    system("clear");
    return 0;
}
