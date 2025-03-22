#pragma once
#include <memory>
#include <string>
#include <vector>

class CameraDevice {
public:

enum class IOMethod {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct Buffer {
    std::shared_ptr<void> start {nullptr};
    size_t length {0};
};

struct FormatInfo {
    int Width {0};
    int Height {0};
};

    CameraDevice() = delete;
    CameraDevice(const std::string &path, FormatInfo formatInfo,
        unsigned int bufferSize, IOMethod io = IOMethod::IO_METHOD_MMAP);
    ~CameraDevice();

    void StartCapturing();
    const Buffer& GetFrame();
    void StopCapturing() noexcept;
    const FormatInfo& GetFormat() const {return FormatInfo_;};

private:
    void OpenDevice(const std::string &path);
    void CloseDevice() noexcept;
    void InitDevice();
    [[nodiscard]] int xioctl (int fd, int request, void* arg);
    void InitReadMode(unsigned int bufSize);
    void InitMmapMode();
    void InitUserPtrMode(unsigned int bufSize);


    int FileDesc_ {-1};
    IOMethod IoMethod_;
    //Todo: We need to use std::memory or/and vector for mem safety
    std::vector<Buffer> Buffers_;
    bool IsCapturing_ {false};
    bool IsFirstFrame_ {true};
    Buffer EmptyBuffer_;

    FormatInfo FormatInfo_;
    unsigned int BufferSize_ {60};
};
