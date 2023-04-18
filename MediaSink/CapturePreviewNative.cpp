#include "CapturePreviewNative.h"

#include "DebuggerLogger.h"
#include "VideoSinkFactory.h"

#include <d3d9.h>
#include <d3d11_1.h>
#include <msclr\lock.h>

#undef Trace
#undef TraceError
#undef CHK
#undef CHKNULL
#undef CHKOOM

//
// Tracing
//

// C++/CLI does not support varargs, so tracing functions are more limited
#define Trace(message) { \
    s_logger.Log(LogLevel::Information, message "\n"); \
}
#define TraceError(message) { \
    s_logger.Log(LogLevel::Error, message "\n"); \
}

//
// Error handling
//

// Exception-based error handling
#define CHK(statement)  {HRESULT _hr = (statement); if (FAILED(_hr)) { throw gcnew System::Runtime::InteropServices::COMException(nullptr, _hr); }}
#define CHKNULL(p)  {if ((p) == nullptr) { CHK(E_POINTER); }}
#define CHKOOM(p)  {if ((p) == nullptr) { CHK(E_OUTOFMEMORY); }}

using namespace msclr;
using namespace System;
using namespace System::Windows;
using namespace System::Windows::Interop;
using namespace System::Windows::Threading;

namespace MediaCapturePreview
{
    namespace
    {
        template<typename T>
        class SampleCallback
        {
        public:
            SampleCallback(T^ parent)
                : m_parent(parent)
            {   }

            void operator()(_In_ IMFSample* sample)
            {   m_parent->OnSample(sample); }

        private:
            msclr::gcroot<T^> m_parent;
        };
    }

    class CapturePreviewNativeState
    {
    public:
        CapturePreviewNativeState(
            unsigned int width,
            unsigned int height,
            _In_ const CComPtr<IMFMediaSink>& sink
        )
            : m_d3d9SharedTextureHandle(nullptr)
            , m_sink(sink)
        {
            Trace("@ initializing DirectX9");

            CHK(Direct3DCreate9Ex(D3D_SDK_VERSION, &m_d3d9));

            D3DPRESENT_PARAMETERS d3dpp = {};
            d3dpp.Windowed = true;
            d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            d3dpp.hDeviceWindow = GetDesktopWindow();
            d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

            CHK(m_d3d9->CreateDeviceEx(
                D3DADAPTER_DEFAULT,
                D3DDEVTYPE_HAL,
                d3dpp.hDeviceWindow,
                D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
                &d3dpp,
                nullptr,
                &m_d3d9Device
            ));

            CHK(m_d3d9Device->CreateTexture(
                width,
                height,
                1,
                D3DUSAGE_RENDERTARGET,
                D3DFMT_X8R8G8B8,
                D3DPOOL_DEFAULT,
                &m_d3d9SharedTexture,
                &m_d3d9SharedTextureHandle
            ));

            CHK(m_d3d9SharedTexture->GetSurfaceLevel(0, &m_d3d9SharedSurface));

            Trace("@ DirectX9 initialized");
        }


        CComPtr<IMFMediaSink> m_sink;
        CComPtr<IDirect3D9Ex> m_d3d9;
        CComPtr<IDirect3DDevice9Ex> m_d3d9Device;
        CComPtr<IDirect3DSurface9> m_d3d9SharedSurface;
        CComPtr<IDirect3DTexture9> m_d3d9SharedTexture;
        CComPtr<IMFSample> m_sample;
        HANDLE m_d3d9SharedTextureHandle;
    };



    CapturePreviewNative::CapturePreviewNative(_In_ D3DImage^ image, unsigned int width, unsigned int height)
        : m_width(width)
        , m_height(height)
        , m_imageInitialized(false)
        , m_image(image)
        , m_state(nullptr)
    {
        Trace("CapturePreviewNative::CapturePreview@ creating the video sink");

        if (image == nullptr)
        {
            throw gcnew ArgumentNullException(L"image");
        }

        CComPtr<IMFMediaSink> sink;
        CreateVideoSink(
            width, 
            height,
            SampleCallback<CapturePreviewNative>(this),
            &sink
            );

        Trace("CapturePreviewNative::CapturePreview@ wrapping the native sink in managed object");

        m_sink = WinRT::MarshalInterface<Object^>::FromAbi(IntPtr(sink.p)); // For .Net Core

    //    m_sink = System::Runtime::InteropServices::Marshal::GetObjectForIUnknown(IntPtr(sink.p)); // For .Net Framework

        m_state = new CapturePreviewNativeState(width, height, sink);
    }

    // IDisposable::Dispose()
    CapturePreviewNative::~CapturePreviewNative()
    {
        lock lock(this);
        Trace("CapturePreviewNative::~CapturePreview@ disposing CapturePreview");

        if (m_image != nullptr)
        {
            m_image->Lock();
            m_image->SetBackBuffer(D3DResourceType::IDirect3DSurface9, IntPtr::Zero);
            m_image->Unlock();
            m_image = nullptr;
            m_imageInitialized = false;
        }

        this->!CapturePreviewNative();
    }

    CapturePreviewNative::!CapturePreviewNative()
    {
        Trace("CapturePreviewNative::!CapturePreview@ finalizing CapturePreview");

        if (m_state != nullptr)
        {
            (void)m_state->m_sink->Shutdown();
            delete m_state;
            m_state = nullptr;
        }
    }

    void CapturePreviewNative::OnSample(_In_ IMFSample* sample)
    {
        Dispatcher^ dispatcher;
        {
            lock lock(this);

            if ((m_image == nullptr) || (m_state == nullptr))
            {
                return;
            }
            if (m_state->m_sample != nullptr)
            {
                return; // Drop frame: UI thread not keeping up
            }
            m_state->m_sample = sample;
            dispatcher = m_image->Dispatcher;
        }

        // Go back to the UI thread
        dispatcher->BeginInvoke(gcnew RefreshPreviewDelegate(this, &CapturePreviewNative::RefreshPreview));
    }

    void CapturePreviewNative::RefreshPreview()
    {
        lock lock(this);

        if ((m_image == nullptr) || (m_state == nullptr))
        {
            return;
        }
        if (!m_image->IsFrontBufferAvailable)
        {
            if (m_imageInitialized)
            {
                Trace("CapturePreviewNative::RefreshPreview@ setting back buffer on D3DImage");

                m_image->Lock();
                m_image->SetBackBuffer(D3DResourceType::IDirect3DSurface9, IntPtr::Zero);
                m_image->Unlock();
                m_imageInitialized = false;
            }
            return;
        }

        if (!m_imageInitialized)
        {
            m_image->Lock();
            m_image->SetBackBuffer(D3DResourceType::IDirect3DSurface9, IntPtr(m_state->m_d3d9SharedSurface.p));
            m_image->Unlock();
            m_imageInitialized = true;
        }

        CComPtr<IMFDXGIBuffer> bufferDxgi;
        CComPtr<IMFMediaBuffer> buffer;
        CHK(m_state->m_sample->GetBufferByIndex(0, &buffer));
        CHK(buffer.QueryInterface(&bufferDxgi));

        CComPtr<IDXGIResource> inputResource;
        unsigned int inputSubresource;
        CHK(bufferDxgi->GetResource(IID_PPV_ARGS(&inputResource)));
        CHK(bufferDxgi->GetSubresourceIndex(&inputSubresource));

        CComPtr<ID3D11Texture2D> inputD3dTexture;
        CComPtr<ID3D11Resource> inputD3dResource;
        CHK(inputResource.QueryInterface(&inputD3dTexture));
        CHK(inputResource.QueryInterface(&inputD3dResource));

        CComPtr<ID3D11Device> inputD3dDevice;
        inputD3dTexture->GetDevice(&inputD3dDevice);

        CComPtr<ID3D11Resource> outputD3dResource;
        CHK(inputD3dDevice->OpenSharedResource(m_state->m_d3d9SharedTextureHandle, IID_PPV_ARGS(&outputD3dResource)));

        Trace("CapturePreviewNative::RefreshPreview@ copying DX texture to D3DImage");

        CComPtr<ID3D11DeviceContext> inputD3dContext;
        inputD3dDevice->GetImmediateContext(&inputD3dContext);
        m_image->Lock();
        inputD3dContext->CopySubresourceRegion(outputD3dResource, 0, 0, 0, 0, inputD3dResource, inputSubresource, nullptr);
        m_image->AddDirtyRect(Int32Rect(0, 0, m_width, m_height));
        m_image->Unlock();

        m_state->m_sample = nullptr;
    }
}
