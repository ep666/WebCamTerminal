#include "Camera.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

#include <linux/videodev2.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>


CameraDevice::CameraDevice(const std::string &path, FormatInfo formatInfo,
    unsigned int bufferSize, IOMethod io) :
    IoMethod_(io),
    BufferSize_(bufferSize),
    FormatInfo_(formatInfo) {
    OpenDevice(path);
    InitDevice();
}


CameraDevice::~CameraDevice() {
    if (IsCapturing_)
        StopCapturing();
    CloseDevice();
}


void CameraDevice::OpenDevice(const std::string &path) {
    struct stat st;

    if (stat(path.c_str(), &st) == -1) {
        throw std::runtime_error("[OpenDevice] Cannot identify " + path);
    }

    if (!S_ISCHR(st.st_mode)) {
        throw std::runtime_error("[OpenDevice] " + path + " is no devicen");
    }

    FileDesc_ = open(path.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

    if (FileDesc_ == -1) {
        throw std::runtime_error("[OpenDevice] Can't open " + path);
    }
}


void CameraDevice::CloseDevice() noexcept {
    close(FileDesc_);
    FileDesc_ = -1;
}


int CameraDevice::xioctl(int fd, int request, void* arg) {
    int r;
    do {
        r = ioctl(fd, request, arg);
    } while (r == -1 && EINTR == errno);
    return r;
}


void CameraDevice::InitDevice() {
    struct v4l2_capability cap;

    if (xioctl(FileDesc_, VIDIOC_QUERYCAP, &cap) == -1) {
        if (EINVAL == errno)
            throw std::runtime_error("[InitDevice] There is no V4L2 device");
        else
            throw std::runtime_error("[InitDevice] VIDIOC_QUERYCAP");
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        throw std::runtime_error("[InitDevice] There is no video capture device");
    }

    switch (IoMethod_) {
        case IOMethod::IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                throw std::runtime_error("[InitDevice] Device doesn't support read i/o");
            }
            break;
        case IOMethod::IO_METHOD_MMAP:
        case IOMethod::IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                throw std::runtime_error("[InitDevice] Device doesn't support streaming i/o");
            }
            break;
    }

    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    std::memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(FileDesc_, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;

        if (xioctl(FileDesc_, VIDIOC_S_CROP, &crop) == -1) {
            switch (errno) {
            case EINVAL:
                std::cout << "Cropping not supported\n";
                break;
            default:
                break;
            }
        }
    } else {
        std::cout << "Errors ignored\n";
    }

    struct v4l2_format fmt;

    std::memset(&fmt, 0, sizeof(fmt));

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = FormatInfo_.Width;
    fmt.fmt.pix.height = FormatInfo_.Height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (xioctl(FileDesc_, VIDIOC_S_FMT, &fmt) == -1) {
        std::runtime_error("[InitDevice] VIDIOC_S_FMT");
    }

    if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV) {
        std::runtime_error("[InitDevice] Unsuported pixelformat");
    }

    FormatInfo_.Height = fmt.fmt.pix.height;
    FormatInfo_.Width = fmt.fmt.pix.width;
    // Buggy driver paranoia.
    unsigned int min;
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (IoMethod_) {
        case IOMethod::IO_METHOD_READ:
            InitReadMode(fmt.fmt.pix.sizeimage);
            break;
        case IOMethod::IO_METHOD_MMAP:
            InitMmapMode();
            break;
        case IOMethod::IO_METHOD_USERPTR:
            InitUserPtrMode(fmt.fmt.pix.sizeimage);
            break;
    }
}


void CameraDevice::InitReadMode(unsigned int bufSize) {
    Buffers_.push_back(Buffer());
    Buffers_[0].length = bufSize;
    Buffers_[0].start = std::shared_ptr<void>(malloc(bufSize), free);
};


void CameraDevice::InitMmapMode() {
    struct v4l2_requestbuffers req;

    std::memset(&req, 0, sizeof(req));
    
    req.count = BufferSize_;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(FileDesc_, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            throw std::runtime_error("[InitMmapMode] Device doesn't support memory mapping");
        } else {
            throw std::runtime_error("[InitMmapMode] VIDIOC_REQBUFS");
        }
    }

    if (req.count < BufferSize_ / 2) {
        throw std::runtime_error("[InitMmapMode] Insufficient buffer memory");
    }

    Buffers_ = std::vector<Buffer>(req.count, Buffer());

    for (__u32 i = 0; i < req.count; ++i) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(FileDesc_, VIDIOC_QUERYBUF, &buf) == -1) {
            throw std::runtime_error("[InitMmapMode] VIDIOC_QUERYBUF");
        }

        auto len = buf.length;
        Buffers_[i].length = len;
        Buffers_[i].start = std::shared_ptr<void>(
            mmap(0, buf.length, PROT_READ | PROT_WRITE,
                 MAP_SHARED, FileDesc_, buf.m.offset
                ), [len] (void* mem) {
                        if (munmap(mem, len) == -1) { 
                            std::terminate();
                    }
                }
        );
    }
};


void CameraDevice::InitUserPtrMode(unsigned int bufSize) {
    struct v4l2_requestbuffers req;

    std::memset(&req, 0, sizeof(req));
    
    req.count = BufferSize_;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;

    if (xioctl(FileDesc_, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            throw std::runtime_error("[InitUserPtrMode] Device doesn't support user pointer i/o");
        } else {
            throw std::runtime_error("[InitUserPtrMode] VIDIOC_REQBUFS");
        }
    }

    Buffers_ = std::vector<Buffer>(req.count, Buffer());

    for (__u32 i = 0; i < req.count; ++i) {
        Buffers_[i].length = bufSize;
        Buffers_[i].start = std::shared_ptr<void>(malloc(bufSize), free);
    }
};


void CameraDevice::StartCapturing() {
    enum v4l2_buf_type type;

    switch(IoMethod_) {
        case IOMethod::IO_METHOD_READ:
            break;
        case IOMethod::IO_METHOD_MMAP:
            for (size_t i = 0; i < Buffers_.size(); ++i) {
                struct v4l2_buffer buf;
                std::memset (&buf, 0, sizeof(buf));

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (xioctl(FileDesc_, VIDIOC_QBUF, &buf) == -1) {
                    throw std::runtime_error("[StartCapturing] VIDIOC_QBUF");
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(FileDesc_, VIDIOC_STREAMON, &type) == -1) {
                throw std::runtime_error("[StartCapturing] VIDIOC_STREAMON");
            }
            break;

        case IOMethod::IO_METHOD_USERPTR:
            for (size_t i = 0; i < Buffers_.size(); ++i) {
                struct v4l2_buffer buf;
                std::memset(&buf, 0, sizeof(buf));

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long)Buffers_[i].start.get();
                buf.length = Buffers_[i].length;

                if (xioctl(FileDesc_, VIDIOC_QBUF, &buf) == -1) {
                    throw std::runtime_error("[StartCapturing] VIDIOC_QBUF");
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(FileDesc_, VIDIOC_STREAMON, &type) == -1) {
                throw std::runtime_error("[StartCapturing] VIDIOC_STREAMON");
            }
            break;
    }

    IsCapturing_ = true;
}


void CameraDevice::StopCapturing() noexcept {
    switch(IoMethod_) {
        case IOMethod::IO_METHOD_READ:
            break;
        case IOMethod::IO_METHOD_MMAP:
        case IOMethod::IO_METHOD_USERPTR:
            auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(FileDesc_, VIDIOC_STREAMOFF, &type) == -1) {
                std::cerr << "VIDIOC_STREAMOFF\n";
            }
            break;
    }
    IsCapturing_ = false;
}


const std::optional<CameraDevice::Buffer> CameraDevice::GetFrame() {
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(FileDesc_, &fds);
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    int retVal = select(FileDesc_ + 1, &fds, 0, 0, &tv);

    if (retVal == 0) {
        throw std::runtime_error("[GetFrame] Device timeout");
    }
    if (retVal == -1) {
        if (EINTR == errno)
            return CameraDevice::GetFrame();
        throw std::runtime_error("[GetFrame] Failed select");
    }

    struct v4l2_buffer buf;
    int index = 0;

    switch (IoMethod_) {
        case IOMethod::IO_METHOD_READ:
            if (read(FileDesc_, Buffers_[0].start.get(), Buffers_[0].length) == -1) {
                if (errno == EAGAIN) {
                    return std::nullopt;
                }
                throw std::runtime_error("[GetFrame] Read failed while getting frame");
            }
            index = 0;
            break;
        case IOMethod::IO_METHOD_MMAP:
            std::memset(&buf, 0, sizeof(buf));

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;

            if (xioctl(FileDesc_, VIDIOC_DQBUF, &buf) == -1) {
                if (errno == EAGAIN)
                    return std::nullopt;
                throw std::runtime_error("[GetFrame] VIDIOC_DQBUF");
            }
            index = buf.index;
            if (xioctl(FileDesc_, VIDIOC_QBUF, &buf) == -1) {
                throw std::runtime_error("[GetFrame] VIDIOC_QBUF");
            }
            break;
        case IOMethod::IO_METHOD_USERPTR:
            std::memset(&buf, 0, sizeof(buf));

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_USERPTR;

            if (xioctl(FileDesc_, VIDIOC_DQBUF, &buf) == -1) {
                if (errno == EAGAIN)
                    return std::nullopt;
                throw std::runtime_error("[GetFrame] VIDIOC_DQBUF");
            }

            for (__u32 i = 0; i < Buffers_.size(); ++i) {
                if (buf.m.userptr == (unsigned long)Buffers_[i].start.get()
                        && buf.length == Buffers_[i].length) {
                        index = i;
                        break;
                }
            }

            if (xioctl(FileDesc_, VIDIOC_QBUF, &buf) == -1) {
                throw std::runtime_error("[GetFrame] VIDIOC_QBUF");
            }
            break;
    }

    return Buffers_[index];
}
