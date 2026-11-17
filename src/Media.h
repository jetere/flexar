#pragma once
#include "Renderer.h"
#include <mfidl.h>
#include <mfreadwrite.h>
namespace flexar {
Image loadImage(const std::filesystem::path& path);
Image demoImage();
void savePng(const std::filesystem::path& path,const Image& image);
class VideoWriter {
public:
    void open(const std::filesystem::path& path,uint32_t width,uint32_t height,uint32_t fps=30);
    void write(const Image& image);
    void finish();
private:
    ComPtr<IMFSinkWriter> writer_;DWORD stream_{};uint32_t width_{},height_{},fps_{};int64_t frame_{};
};
class Video {
public:
    void open(const std::filesystem::path& path);
    bool frame(double seconds,Image& image);
    void seek(double seconds);
    void close(){reader_.Reset();pending_={};duration=0;ended_=false;}
    bool loaded()const{return reader_!=nullptr;}
    double duration{};
private:
    bool read(Image& image,double& stamp);
    ComPtr<IMFSourceReader> reader_;
    uint32_t width_{},height_{};LONG stride_{};
    Image pending_;double pendingTime_{},lastRequest_{-1};bool ended_{};
};
}
