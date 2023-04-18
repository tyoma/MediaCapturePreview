#pragma once

#include <functional>
#include <mfidl.h>

//
// WRL is blocked in C++/CLI projects, so split media sinks out into a static lib
//
namespace MediaCapturePreview
{
    void CreateVideoSink(unsigned int width, unsigned int height, const std::function<void(IMFSample*)>& sampleHandler, _COM_Outptr_ IMFMediaSink** sink);
}
