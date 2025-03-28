#pragma once
#include "Camera.h"
#include <iostream>
#include <cmath>


using Buffer = CameraDevice::Buffer;

namespace FrameConverter {

const std::string grayScale92 = " `.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$Bg0MNWQ%&@";
const std::string grayScale70 = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B@$";
const std::string grayScale10 = " .:-=+*#%@";

struct YUYV {
    unsigned char b0;
    unsigned char b1;
};


inline void ConvertAndPrint(const Buffer& buffer, int height, int width,
                             const std::string& grayScale) {
    if (buffer.length <= 0 || !buffer.start)
        return;
    YUYV* bufArray = reinterpret_cast<YUYV*>(buffer.start.get());
    std::string frameLine;    
    for (int i = 20; i < height - 20; ++i) {
        frameLine.clear();
        for (int j = 0; j < width; ++j) {
            unsigned char m1 = 1;
            unsigned char m2 = 1 << 2;
            unsigned char m3 = 1 << 4;
            unsigned char m4 = 1 << 6;
            unsigned char val = 0;
    
            val |= (m1 & bufArray[i * width + j].b0);
            val |= (m2 & bufArray[i * width + j].b0) >> 1;
            val |= (m3 & bufArray[i * width + j].b0) >> 2;
            val |= (m4 & bufArray[i * width + j].b0) >> 3;
            val |= (m1 & bufArray[i * width + j].b1) << 4;
            val |= (m2 & bufArray[i * width + j].b1) << 3;
            val |= (m3 & bufArray[i * width + j].b1) << 2;
            val |= (m4 & bufArray[i * width + j].b1) << 1;
                
            double convertedVal = double(val) / 255;
            size_t index = std::floor(convertedVal * (grayScale.size() - 1));
            if (index > grayScale.size()) {
                std::cout << "Index: " << index << std::endl;
                std::cout << convertedVal << ": convertedVal" << std::endl;
                std::cout << val << ": val" << std::endl;
                throw std::runtime_error("[FrameConverter] Out of bound index");
            }
            frameLine += grayScale[index];
        }
        std::cout << frameLine << '\n';
    }
}
}
