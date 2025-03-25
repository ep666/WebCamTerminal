#pragma once
#include "Camera.h"
#include "FrameConverter.h"

struct CameraParams {
    std::string DevicePath = "/dev/video0";
    CameraDevice::IOMethod IoMethod = CameraDevice::IOMethod::IO_METHOD_MMAP;
    CameraDevice::FormatInfo FormatInfo = {160, 120};
    std::string GrayScaleDepth = FrameConverter::grayScale10;
    unsigned int BufferSize = 4;
};

void Configure(CameraParams& params);
