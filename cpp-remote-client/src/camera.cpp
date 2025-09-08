#include "camera.h"
#include <iostream>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <dshow.h>
#pragma comment(lib, "strmiids.lib")

// Windows implementation: Camera methods
Camera::Camera() : /* members */ pGraph(nullptr), pCapture(nullptr), pControl(nullptr) {
    CoInitialize(nullptr);
}

Camera::~Camera() {
    if (pGraph) pGraph->Release();
    if (pCapture) pCapture->Release();
    if (pControl) pControl->Release();
    CoUninitialize();
}

bool Camera::initialize() {
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraph);
    if (FAILED(hr)) return false;
    hr = pGraph->QueryInterface(IID_IMediaControl, (void**)&pControl);
    if (FAILED(hr)) return false;
    return true;
}

void Camera::startStreaming() {
    if (pControl) pControl->Run();
}

void Camera::stopStreaming() {
    if (pControl) pControl->Stop();
}

std::vector<unsigned char> Camera::captureFrame() {
    return std::vector<unsigned char>(); // placeholder
}

#else
#include <opencv2/opencv.hpp>

// POSIX implementation
Camera::Camera() : cap(0) {}

Camera::~Camera() { cap.release(); }

bool Camera::initialize() { return cap.isOpened(); }

void Camera::startStreaming() {
    cv::Mat frame;
    while (cap.isOpened()) {
        cap >> frame;
        if (frame.empty()) break;
        // process
    }
}

void Camera::stopStreaming() { cap.release(); }

std::vector<unsigned char> Camera::captureFrame() {
    return std::vector<unsigned char>(); // placeholder
}

#endif

// free function convenience
void access_camera() {
    Camera camera;
    if (!camera.initialize()) {
        std::cerr << "Camera initialization failed." << std::endl;
        return;
    }
    camera.startStreaming();
}