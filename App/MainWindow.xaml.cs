using MediaCapturePreview;
using System;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Interop;
using Windows.Media;
using Windows.Media.Capture;
using Windows.Media.MediaProperties;

namespace VideoPreview
{
    class D3DCameraPreview : IDisposable
    {
        D3DCameraPreview(MediaCapture mediaCapture, CapturePreviewNative imageSink)
        {
            _mediaCapture = mediaCapture;
            _imageSink = imageSink;
        }

        public void Dispose()
        {
            _imageSink.Dispose();
            _mediaCapture.Dispose();
        }

        public static async Task<IDisposable> BeginPreview(D3DImage target)
        {
            var capture = new MediaCapture();
            CapturePreviewNative imageSink = null!;

            try
            {
                await capture.InitializeAsync(new MediaCaptureInitializationSettings {
                    StreamingCaptureMode = StreamingCaptureMode.Video,
                    // VideoDeviceId = ...,
                });

                var props = (VideoEncodingProperties)capture.VideoDeviceController.GetMediaStreamProperties(MediaStreamType.VideoPreview);

                imageSink = new CapturePreviewNative(target, props.Width, props.Height);
                await capture.StartPreviewToCustomSinkAsync(new MediaEncodingProfile {
                    Audio = null,
                    Video = VideoEncodingProperties.CreateUncompressed(MediaEncodingSubtypes.Yuy2, props.Width, props.Height),
                    Container = null
                }, (IMediaExtension)imageSink.MediaSink);
                return new D3DCameraPreview(capture, imageSink);
            }
            catch
            {
                imageSink?.Dispose();
                capture.Dispose();
                throw;
            }
        }

        MediaCapture _mediaCapture;
        CapturePreviewNative _imageSink;
    }

    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            _preview.Source = _previewImage;
        }

        protected override async void OnInitialized(EventArgs e)
        {
            base.OnInitialized(e);
            _previewSink = await D3DCameraPreview.BeginPreview(_previewImage);
        }

        protected override void OnClosed(EventArgs e)
        {
            base.OnClosed(e);
            _previewSink?.Dispose();
        }

        D3DImage _previewImage = new();
        IDisposable? _previewSink;
    }
}
