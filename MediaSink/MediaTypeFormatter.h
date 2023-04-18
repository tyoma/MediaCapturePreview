#pragma once

#include <mfapi.h>
#include <string>
#include <windows.h>
#include <windows.media.h>
#include <wrl.h>

namespace MediaCapturePreview
{
    class MediaTypeFormatter
    {
    public:

        static std::string Format(_In_opt_ const ::Microsoft::WRL::ComPtr<IMFMediaType>& mt);

        static void AddAttribute(_In_ const ::Microsoft::WRL::ComPtr<IMFAttributes>& attr, _In_ unsigned int i, _Inout_ std::ostringstream& stream);
        static void AddGuid(_In_ const GUID& guid, _Inout_ std::ostringstream& stream);

    private:

        static LPCSTR _GetGuidFriendlyName(_In_ const GUID& guid);
    };
}
