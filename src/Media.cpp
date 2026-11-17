#include "Media.h"
#include <wincodec.h>
#include <mfapi.h>
#include <mferror.h>
#include <propvarutil.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace flexar {
void savePng(const std::filesystem::path& path,const Image& image){
    if(!image.width||!image.height||image.pixels.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Invalid export image");
    ComPtr<IWICImagingFactory> factory;verify(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;verify(factory->CreateStream(&stream));verify(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;verify(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));verify(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;verify(encoder->CreateNewFrame(&frame,nullptr));verify(frame->Initialize(nullptr));verify(frame->SetSize(image.width,image.height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;verify(frame->SetPixelFormat(&format));if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("PNG encoder does not accept BGRA");
    auto pixels=image.pixels;for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    verify(frame->WritePixels(image.height,image.width*4,static_cast<UINT>(pixels.size()),pixels.data()));verify(frame->Commit());verify(encoder->Commit());
}
void VideoWriter::open(const std::filesystem::path& path,uint32_t width,uint32_t height,uint32_t fps){
    if(!width||!height||!fps)throw std::runtime_error("Invalid video export dimensions");
    width_=(width+1)&~1u;height_=(height+1)&~1u;fps_=fps;frame_=0;
    verify(MFCreateSinkWriterFromURL(path.c_str(),nullptr,nullptr,&writer_));
    ComPtr<IMFMediaType> output;verify(MFCreateMediaType(&output));verify(output->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));verify(output->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264));
    verify(output->SetUINT32(MF_MT_AVG_BITRATE,16000000));verify(output->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));verify(MFSetAttributeSize(output.Get(),MF_MT_FRAME_SIZE,width_,height_));verify(MFSetAttributeRatio(output.Get(),MF_MT_FRAME_RATE,fps_,1));verify(MFSetAttributeRatio(output.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));
    verify(writer_->AddStream(output.Get(),&stream_));ComPtr<IMFMediaType> input;verify(MFCreateMediaType(&input));verify(output->CopyAllItems(input.Get()));verify(input->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32));verify(input->SetUINT32(MF_MT_DEFAULT_STRIDE,width_*4));verify(writer_->SetInputMediaType(stream_,input.Get(),nullptr));verify(writer_->BeginWriting());
}
void VideoWriter::write(const Image& image){
    if(!writer_||!image.width||!image.height||image.width>width_||image.height>height_||image.pixels.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Invalid video export frame");
    ComPtr<IMFMediaBuffer> buffer;verify(MFCreateMemoryBuffer(width_*height_*4,&buffer));BYTE* data;verify(buffer->Lock(&data,nullptr,nullptr));
    for(UINT y=0;y<height_;++y)for(UINT x=0;x<width_;++x){auto src=image.pixels.data()+(size_t(std::min(y,image.height-1))*image.width+std::min(x,image.width-1))*4;auto dst=data+(size_t(y)*width_+x)*4;float alpha=src[3]/255.f;dst[0]=BYTE(src[2]*alpha);dst[1]=BYTE(src[1]*alpha);dst[2]=BYTE(src[0]*alpha);dst[3]=255;}
    verify(buffer->Unlock());verify(buffer->SetCurrentLength(width_*height_*4));ComPtr<IMFSample> sample;verify(MFCreateSample(&sample));verify(sample->AddBuffer(buffer.Get()));
    auto start=frame_*10000000/fps_,end=(frame_+1)*10000000/fps_;verify(sample->SetSampleTime(start));verify(sample->SetSampleDuration(end-start));verify(writer_->WriteSample(stream_,sample.Get()));++frame_;
}
void VideoWriter::finish(){if(writer_){auto writer=std::move(writer_);verify(writer->Finalize());}}
Image loadImage(const std::filesystem::path& path) {
    ComPtr<IWICImagingFactory> factory;verify(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;verify(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;verify(decoder->GetFrame(0,&frame));
    UINT w,h;verify(frame->GetSize(&w,&h));
    // Bound editor working resolution and VRAM allocation for large artwork.
    float scale=std::min(1.f,1920.f/std::max(w,h));
    Image result;result.width=std::max(1u,UINT(w*scale));result.height=std::max(1u,UINT(h*scale));
    ComPtr<IWICBitmapScaler> scaler;verify(factory->CreateBitmapScaler(&scaler));verify(scaler->Initialize(frame.Get(),result.width,result.height,WICBitmapInterpolationModeFant));
    ComPtr<IWICFormatConverter> convert;verify(factory->CreateFormatConverter(&convert));verify(convert->Initialize(scaler.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    result.pixels.resize(size_t(result.width)*result.height*4);verify(convert->CopyPixels(nullptr,result.width*4,static_cast<UINT>(result.pixels.size()),result.pixels.data()));return result;
}
Image demoImage(){
    Image im{960,540};im.pixels.resize(size_t(im.width)*im.height*4);
    for(uint32_t y=0;y<im.height;++y)for(uint32_t x=0;x<im.width;++x){float u=float(x)/im.width,v=float(y)/im.height;float ring=std::sin(std::sqrt((u-.5f)*(u-.5f)+(v-.5f)*(v-.5f))*65);float bars=std::sin(u*30+v*5);size_t i=(size_t(y)*im.width+x)*4;im.pixels[i]=uint8_t(std::clamp(.45f+.4f*ring,0.f,1.f)*255);im.pixels[i+1]=uint8_t(std::clamp(.35f+.3f*bars,0.f,1.f)*255);im.pixels[i+2]=uint8_t((.3f+.65f*u)*255);im.pixels[i+3]=255;}return im;
}
void Video::open(const std::filesystem::path& path){
    close();ComPtr<IMFAttributes> attributes;verify(MFCreateAttributes(&attributes,2));verify(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING,TRUE));
    verify(MFCreateSourceReaderFromURL(path.c_str(),attributes.Get(),&reader_));
    verify(reader_->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE));verify(reader_->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE));
    ComPtr<IMFMediaType> type;verify(MFCreateMediaType(&type));verify(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));verify(type->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32));verify(reader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,nullptr,type.Get()));
    verify(reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&type));verify(MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&width_,&height_));
    if(!width_||!height_||width_>4096||height_>4096)throw std::runtime_error("Video dimensions must be at most 4096 x 4096 for this preview build");
    UINT32 stride;if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stride)))stride_=static_cast<LONG>(stride);else verify(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1,width_,&stride_));
    PROPVARIANT value;PropVariantInit(&value);if(SUCCEEDED(reader_->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE,MF_PD_DURATION,&value))&&value.vt==VT_UI8)duration=double(value.uhVal.QuadPart)/1e7;PropVariantClear(&value);
    lastRequest_=-1;ended_=false;
}
void Video::seek(double seconds){if(!reader_)return;PROPVARIANT p;PropVariantInit(&p);p.vt=VT_I8;p.hVal.QuadPart=static_cast<LONGLONG>(std::max(0.,seconds)*1e7);verify(reader_->SetCurrentPosition(GUID_NULL,p));pending_={};ended_=false;lastRequest_=seconds;}
bool Video::read(Image& image,double& stamp){
    for(int attempt=0;attempt<100;++attempt){DWORD flags=0;LONGLONG timestamp=0;ComPtr<IMFSample> sample;
        verify(reader_->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,nullptr,&flags,&timestamp,&sample));
        if(flags&MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED){
            ComPtr<IMFMediaType> type;verify(reader_->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&type));
            GUID subtype;verify(type->GetGUID(MF_MT_SUBTYPE,&subtype));if(subtype!=MFVideoFormat_RGB32)throw std::runtime_error("Decoder changed to an unsupported pixel format");
            verify(MFGetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,&width_,&height_));
            if(!width_||!height_||width_>4096||height_>4096)throw std::runtime_error("Video exceeds the 4096 x 4096 preview limit");
            UINT32 stride;if(SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE,&stride)))stride_=static_cast<LONG>(stride);else verify(MFGetStrideForBitmapInfoHeader(MFVideoFormat_RGB32.Data1,width_,&stride_));
        }
        if(flags&MF_SOURCE_READERF_ENDOFSTREAM){ended_=true;return false;}if(!sample)continue;
        ComPtr<IMFMediaBuffer> buffer;verify(sample->ConvertToContiguousBuffer(&buffer));BYTE* data;DWORD length;verify(buffer->Lock(&data,nullptr,&length));
        size_t pitch=size_t(std::abs(stride_));if(length<pitch*height_){buffer->Unlock();throw std::runtime_error("Video frame buffer is truncated");}
        image.width=width_;image.height=height_;image.pixels.resize(size_t(width_)*height_*4);
        for(UINT y=0;y<height_;++y){const BYTE* row=data+size_t(stride_<0?height_-1-y:y)*pitch;auto dst=image.pixels.data()+size_t(y)*width_*4;for(UINT x=0;x<width_;++x){dst[x*4]=row[x*4+2];dst[x*4+1]=row[x*4+1];dst[x*4+2]=row[x*4];dst[x*4+3]=255;}}
        buffer->Unlock();stamp=double(timestamp)/1e7;return true;
    }throw std::runtime_error("Decoder produced no frame");
}
bool Video::frame(double seconds,Image& image){
    if(!reader_)return false;if(seconds<lastRequest_||seconds-lastRequest_>.5)seek(seconds);lastRequest_=seconds;
    bool changed=false;
    // Retain one future frame; never present it early. Decode through a seek's keyframe preroll.
    for(int i=0;i<1000;++i){if(pending_.pixels.empty()){if(ended_||!read(pending_,pendingTime_))break;}
        if(pendingTime_>seconds&&changed)break;
        if(pendingTime_>seconds&&!image.pixels.empty())break;
        image=std::move(pending_);pending_={};changed=true;
    }return changed;
}
}
