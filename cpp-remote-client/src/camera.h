#ifndef CAMERA_H
#define CAMERA_H

#include <vector>

#if defined(_WIN32)
// Forward-declare DirectShow interfaces in the global namespace so camera.cpp can use the real types
struct IGraphBuilder;
struct ICaptureGraphBuilder2;
struct IMediaControl;
#endif

class Camera {
public:
    Camera();
    ~Camera();

    bool initialize();
    void startStreaming();
    void stopStreaming();
    std::vector<unsigned char> captureFrame();

private:
    // Platform-specific private members
#if defined(_WIN32)
    IGraphBuilder* pGraph; // IGraphBuilder*
    ICaptureGraphBuilder2* pCapture; // ICaptureGraphBuilder2*
    IMediaControl* pControl; // IMediaControl*
#else
    // OpenCV capture object
    // forward declare as void* to avoid pulling OpenCV into header for all builds
    void* cap; // cv::VideoCapture*
#endif
};

// Convenience free-function access used by camera.cpp
void access_camera();

#endif // CAMERA_H