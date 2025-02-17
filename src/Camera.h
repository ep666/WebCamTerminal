#include <string>

class CameraDevice {
public:

enum class IOMethod {
    IO_METHOD_READ,
    IO_METHOD_MMAP,
    IO_METHOD_USERPTR,
};

struct Buffer {
    void *start;
    size_t length;
};

    CameraDevice() = delete;
    CameraDevice(const std::string &path, 
        IOMethod io = IOMethod::IO_METHOD_MMAP);
    ~CameraDevice();

    void StartCapturing();
    void GetFrame();
    void StopCapturing() noexcept;

private:
    void OpenDevice(const std::string &path);
    void CloseDevice() noexcept;
    void InitDevice();
    void DeinitDevice() noexcept;
    [[nodiscard]] int xioctl (int fh, int request, void* arg);
    void InitReadMode(unsigned int bufSize);
    void InitMmapMode();
    void InitUserPtrMode(unsigned int bufSize);


    int FileDesc {-1};
    IOMethod IoMethod;
    //Todo: We need to use std::memory or/and vector for mem safety
    Buffer* Buffers {nullptr};
    size_t NumBuffers {0};
    bool isCapturing {false};
};