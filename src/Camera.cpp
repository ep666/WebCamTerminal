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


CameraDevice::CameraDevice(const std::string &path, IOMethod io) 
    : IoMethod(io) {
    OpenDevice(path);
    InitDevice();
}


CameraDevice::~CameraDevice() {
    if (isCapturing)
        StopCapturing();
    DeinitDevice();
    CloseDevice();
}


void CameraDevice::OpenDevice(const std::string &path) {
    struct stat st;

    if (stat(path.c_str(), &st) == -1) {
        throw std::runtime_error("Cannot identify " + path);
    }

    if (!S_ISCHR(st.st_mode)) {
        throw std::runtime_error(path + " is no devicen");
    }

    FileDesc = open(path.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

    if (FileDesc == -1) {
        throw std::runtime_error("Can't open " + path);
    }
}


void CameraDevice::CloseDevice() noexcept {
    close(FileDesc);
    FileDesc = -1;
}


int CameraDevice::xioctl(int fh, int request, void* arg) {
    int r;
    do {
        r = ioctl(fh, request, arg);
    } while (r == -1 && EINTR == errno);
    return r;
}


void CameraDevice::InitDevice() {
    struct v4l2_capability cap;

    if (xioctl(FileDesc, VIDIOC_QUERYCAP, &cap) == -1) {
        if (EINVAL == errno)
            throw std::runtime_error("There is no V4L2 device");
        else
            throw std::runtime_error("VIDIOC_QUERYCAP");
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        throw std::runtime_error("There is no video capture device");
    }

    switch (IoMethod) {
        case IOMethod::IO_METHOD_READ:
            if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                throw std::runtime_error("Device doesn't support read i/o");
            }
            break;
        case IOMethod::IO_METHOD_MMAP:
        case IOMethod::IO_METHOD_USERPTR:
            if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                throw std::runtime_error("Device doesn't support streaming i/o");
            }
            break;
    }

    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;

    std::memset(&cropcap, 0, sizeof(cropcap));
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (xioctl(FileDesc, VIDIOC_CROPCAP, &cropcap) == 0) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect;

        if (xioctl(FileDesc, VIDIOC_S_CROP, &crop)) {
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
    //Todo: make it possible to change format;
    fmt.fmt.pix.width = 640;
    fmt.fmt.pix.height = 480;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

    if (xioctl(FileDesc, VIDIOC_S_FMT, &fmt)) {
        std::runtime_error("VIDIOC_S_FMT");
    }

    // Buggy driver paranoia.
    unsigned int min;
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
        fmt.fmt.pix.bytesperline = min;
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
        fmt.fmt.pix.sizeimage = min;

    switch (IoMethod) {
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


void CameraDevice::DeinitDevice() noexcept {
    switch (IoMethod) {
        case IOMethod::IO_METHOD_READ:
            free(Buffers[0].start);
            break;
        case IOMethod::IO_METHOD_MMAP:
            for (size_t i = 0; i < NumBuffers; ++i) {
                //peace of shit code. Must be called in destructor w/o exception;
                //for now terminating the program;
                if (munmap(Buffers[i].start, Buffers[i].length) == -1) {
                    std::terminate();
                }
            }
            break;
        case IOMethod::IO_METHOD_USERPTR:
            for (size_t i = 0; i < NumBuffers; ++i) {
                free(Buffers[i].start);;
            }
            break;
    }

    free(Buffers);
}


void CameraDevice::InitReadMode(unsigned int bufSize) {
    Buffers = (Buffer*)calloc(1, sizeof(*Buffers));

    if (!Buffers) {
        throw std::bad_alloc();
    }

    Buffers[0].length = bufSize;
    Buffers[0].start = malloc(bufSize);

    if (!Buffers[0].start) {
        throw std::bad_alloc();
    }
};


void CameraDevice::InitMmapMode() {
    struct v4l2_requestbuffers req;

    std::memset(&req, 0, sizeof(req));
    
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(FileDesc, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            throw std::runtime_error("Device doesn't support memory mapping");
        } else {
            throw std::runtime_error("VIDIOC_REQBUFS");
        }
    }

    if (req.count < 2) {
        throw std::runtime_error("Insufficient buffer memory");
    }

    Buffers = (Buffer*)calloc(req.count, sizeof(*Buffers));

    if (!Buffers) {
        throw std::bad_alloc();
    }

    for (NumBuffers = 0; NumBuffers < req.count; ++NumBuffers) {
        struct v4l2_buffer buf;
        std::memset(&buf, 0, sizeof(buf));

        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = NumBuffers;

        if (xioctl(FileDesc, VIDIOC_QUERYBUF, &buf) == -1) {
            throw std::runtime_error("VIDIOC_QUERYBUF");
        }

        Buffers[NumBuffers].length = buf.length;
        Buffers[NumBuffers].start = mmap(0, buf.length, 
            PROT_READ | PROT_WRITE, MAP_SHARED, FileDesc, buf.m.offset);

        if (Buffers[NumBuffers].start == MAP_FAILED) {
            throw std::runtime_error("mmap failed");
        }
    }
};


void CameraDevice::InitUserPtrMode(unsigned int bufSize) {
    struct v4l2_requestbuffers req;

    std::memset(&req, 0, sizeof(req));
    
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(FileDesc, VIDIOC_REQBUFS, &req) == -1) {
        if (errno == EINVAL) {
            throw std::runtime_error("Device doesn't support user pointer i/o");
        } else {
            throw std::runtime_error("VIDIOC_REQBUFS");
        }
    }

    Buffers = (Buffer*)calloc(4, sizeof(*Buffers));

    if (!Buffers) {
        throw std::bad_alloc();
    }

    for (NumBuffers = 0; NumBuffers < 4; ++NumBuffers) {
        Buffers[NumBuffers].length = bufSize;
        Buffers[NumBuffers].start = malloc(bufSize);

        if (!Buffers[NumBuffers].start) {
            throw std::bad_alloc();
        }
    }
};

void CameraDevice::StartCapturing() {
    enum v4l2_buf_type type;

    switch(IoMethod) {
        case IOMethod::IO_METHOD_READ:
            break;
        case IOMethod::IO_METHOD_MMAP:
            for (size_t i = 0; i < NumBuffers; ++i) {
                struct v4l2_buffer buf;
                std::memset (&buf, 0, sizeof(buf));

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (xioctl(FileDesc, VIDIOC_QBUF, &buf) == -1) {
                    throw std::runtime_error("VIDIOC_QBUF");
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(FileDesc, VIDIOC_STREAMON, &type) == -1) {
                throw std::runtime_error("VIDIOC_STREAMON");
            }
            break;

        case IOMethod::IO_METHOD_USERPTR:
            for (size_t i = 0; i < NumBuffers; ++i) {
                struct v4l2_buffer buf;
                std::memset(&buf, 0, sizeof(buf));

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;
                buf.index = i;
                buf.m.userptr = (unsigned long)Buffers[i].start;
                buf.length = Buffers[i].length;

                if (xioctl(FileDesc, VIDIOC_QBUF, &buf)) {
                    throw std::runtime_error("VIDIOC_QBUF");
                }
            }
            type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(FileDesc, VIDIOC_STREAMON, &type) == -1) {
                throw std::runtime_error("VIDIOC_STREAMON");
            }
            break;
    }

    isCapturing = true;
}
void CameraDevice::StopCapturing() noexcept {
    switch(IoMethod) {
        case IOMethod::IO_METHOD_READ:
            break;
        case IOMethod::IO_METHOD_MMAP:
        case IOMethod::IO_METHOD_USERPTR:
            auto type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (xioctl(FileDesc, VIDIOC_STREAMOFF, &type) == -1) {
                std::cerr << "VIDIOC_STREAMOFF\n";
                //std::terminate();
            }
            break;
    }
    isCapturing = false;
}
