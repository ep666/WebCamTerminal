#include "Params.h"

#include <filesystem>
#include <limits>

class ParamsConfigurator {
public:
  ParamsConfigurator() = delete;
  ParamsConfigurator(CameraParams &params) : Params_(params) {}

  void Configure() {
    char input;
    while (true) {
      PrintInfo();
      std::cin >> input;
      switch (input) {
      case 'c':
        return;
        break;
      case 'q':
        exit(0);
        break;
      case '1':
        ChangeDevice();
        break;
      case '2':
        ChangeIOMethod();
        break;
      case '3':
        ChangeResolution();
        break;
      case '4':
        ChangeGrayScaleDepth();
        break;
      case '5':
        ChangeBufferSize();
        break;
      default:
        std::cout << "Wrong input\n";
        break;
      }
    }
  }

private:
  void PrintInfo() {
    std::cout << "====Camera configuration====\n";
    std::cout << "Type 1 to change path to device. Current path: "
              << Params_.DevicePath << std::endl;
    std::cout << "Type 2 to change I/O method. Current method: "
              << static_cast<int>(Params_.IoMethod) << std::endl;
    std::cout << "Type 3 to change resolution. Current resoslution: "
              << Params_.FormatInfo.Width << "x" << Params_.FormatInfo.Height
              << std::endl;
    std::cout << "Type 4 to change grayscale depth. Current depth: "
              << Params_.GrayScaleDepth.size() << std::endl;
    std::cout << "Type 5 to change buffer size. Current buffer size: "
              << Params_.BufferSize << std::endl;
    std::cout << "Type c to continue" << std::endl;
    std::cout << "Type q to quit" << std::endl;
  }

  void ChangeDevice() {
    const std::string path = "/dev";
    std::vector<std::string> paths;
    for (const auto &entry : std::filesystem::directory_iterator(path)) {
      if (entry.path().string().find("video") != std::string::npos)
        paths.push_back(entry.path());
    }
    if (paths.empty()) {
      std::cout << "There is no device connected" << std::endl;
      return;
    }
    char input;
    while (true) {
      std::cout << "Type number of desired device.\n"
                   "If there is no variants, "
                   "check that video device is connected"
                << std::endl;

      for (size_t i = 0; i < paths.size(); ++i) {
        std::cout << "[" << i << "]: " << paths[i] << std::endl;
      }

      std::cin >> input;

      if (isdigit(input)) {
        size_t idx = input - '0';
        if (idx < paths.size()) {
          Params_.DevicePath = paths[idx];
          return;
        }
      }
      std::cout << "Wrong input" << std::endl;
    }
  }

  void ChangeIOMethod() {
    char input;
    while (true) {
      std::cout << "Select I/O method: \n"
                << "[0] Read method \n"
                << "[1] mmap method \n"
                << "[2] User ptr method\n";

      std::cin >> input;

      if (input >= '0' && input <= '2') {
        Params_.IoMethod = (CameraDevice::IOMethod)(input - '0');
        return;
      }

      std::cout << "Wrong input" << std::endl;
    }
  }

  void ChangeResolution() {
    while (true) {
      std::cout << "Enter width\n";

      int width = -1;
      std::cin >> width;

      std::cout << "Enter height\n";

      int height = -1;
      std::cin >> height;

      if (width > 0 && height > 0) {
        Params_.FormatInfo = {width, height};
        return;
      }

      std::cout << "Wrong input" << std::endl;
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
  }

  void ChangeGrayScaleDepth() {
    while (true) {
      std::cout << "Select grayscale depth:\n"
                << "[0] GrayScale 10 \n"
                << "[1] GrayScale 70 \n"
                << "[2] GrayScale 92 \n";

      int input = -1;

      std::cin >> input;

      if (input >= 0 && input <= 2) {
        Params_.GrayScaleDepth = input == 0   ? FrameConverter::grayScale10
                                 : input == 1 ? FrameConverter::grayScale70
                                              : FrameConverter::grayScale92;
        return;
      }

      std::cout << "Wrong input" << std::endl;
    }
  }

  void ChangeBufferSize() {
    while (true) {
      std::cout << "Enter buffer size" << std::endl;

      int input = -1;

      std::cin >> input;

      if (input > 0) {
        Params_.BufferSize = input;
        return;
      }
      std::cin.clear();
      std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
      std::cout << "Wrong input" << std::endl;
    }
  }

  CameraParams &Params_;
};

void Configure(CameraParams &params) {
  ParamsConfigurator configurator(params);
  configurator.Configure();
}
