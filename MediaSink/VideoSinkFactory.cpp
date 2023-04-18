#include "VideoSinkFactory.h"

#include "DebuggerLogger.h"
#include "MediaSink.h"

#include <mfapi.h>

using namespace ABI::Windows::Media;
using namespace Microsoft::WRL;
using namespace std;

namespace MediaCapturePreview
{
    void CreateVideoSink(unsigned int width, unsigned int height, const function<void(IMFSample*)>& sampleHandler, _COM_Outptr_ IMFMediaSink** sink)
    {
        *sink = nullptr;

        ComPtr<IMFMediaType> mt;
        CHK(MFCreateMediaType(&mt));
        CHK(mt->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
        CHK(mt->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32));
        CHK(MFSetAttributeSize(mt.Get(), MF_MT_FRAME_SIZE, width, height));

        ComPtr<MediaSink> mediaSink = Make<MediaSink>(
            nullptr,
            mt,
            [](IMFSample*){},
            sampleHandler
            );
        CHKOOM(mediaSink);

        *sink = mediaSink.Detach();
    }
}
