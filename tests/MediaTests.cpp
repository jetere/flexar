#include "Media.h"
#include <mfapi.h>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <fstream>
using namespace flexar;
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
int main(){
    try{
        verify(CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED));verify(MFStartup(MF_VERSION));
        {
            const auto still=std::filesystem::absolute("image-fixture.bmp");
            BITMAPFILEHEADER file{};file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(BITMAPINFOHEADER);file.bfSize=file.bfOffBits+64*32*4;
            BITMAPINFOHEADER info{};info.biSize=sizeof(info);info.biWidth=64;info.biHeight=-32;info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;
            std::vector<uint8_t> pixels(64*32*4);for(size_t i=0;i<pixels.size();i+=4){pixels[i]=25;pixels[i+1]=90;pixels[i+2]=210;pixels[i+3]=255;}
            std::ofstream output(still,std::ios::binary);output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());output.close();
            auto image=loadImage(still);check(image.width==64&&image.height==32,"Image import dimensions incorrect");check(image.pixels[0]==210&&image.pixels[1]==90&&image.pixels[2]==25&&image.pixels[3]==255,"Image RGBA import incorrect");
            image.pixels[3]=128;savePng("export-test.png",image);auto exported=loadImage("export-test.png");check(exported.pixels==image.pixels,"PNG export must preserve RGBA");
            VideoWriter writer;Image movieFrame=image;movieFrame.height=64;movieFrame.pixels.resize(64*64*4);for(size_t p=0;p<movieFrame.pixels.size();p+=4){movieFrame.pixels[p]=210;movieFrame.pixels[p+1]=90;movieFrame.pixels[p+2]=25;movieFrame.pixels[p+3]=128;}writer.open("export-test.mp4",64,64);for(int i=0;i<30;++i)writer.write(movieFrame);writer.finish();Video movie;movie.open("export-test.mp4");Image decoded;check(movie.frame(0,decoded),"MP4 export cannot decode");check(decoded.width==64&&decoded.height==64,"MP4 export dimensions incorrect");check(std::abs(int(decoded.pixels[0])-105)<12,"MP4 alpha compositing incorrect");
        }
        const auto path=std::filesystem::absolute("decode-fixture.mp4");
        {
            ComPtr<IMFSinkWriter> writer;verify(MFCreateSinkWriterFromURL(path.c_str(),nullptr,nullptr,&writer));
            ComPtr<IMFMediaType> type;verify(MFCreateMediaType(&type));
            verify(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Video));verify(type->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_H264));
            verify(type->SetUINT32(MF_MT_AVG_BITRATE,1000000));verify(type->SetUINT32(MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive));
            verify(MFSetAttributeSize(type.Get(),MF_MT_FRAME_SIZE,64,64));verify(MFSetAttributeRatio(type.Get(),MF_MT_FRAME_RATE,30,1));verify(MFSetAttributeRatio(type.Get(),MF_MT_PIXEL_ASPECT_RATIO,1,1));
            DWORD stream;verify(writer->AddStream(type.Get(),&stream));ComPtr<IMFMediaType> input;verify(MFCreateMediaType(&input));verify(type->CopyAllItems(input.Get()));verify(input->SetGUID(MF_MT_SUBTYPE,MFVideoFormat_RGB32));verify(writer->SetInputMediaType(stream,input.Get(),nullptr));verify(writer->BeginWriting());
            for(int frame=0;frame<60;++frame){
                ComPtr<IMFMediaBuffer> buffer;verify(MFCreateMemoryBuffer(64*64*4,&buffer));BYTE* pixels;verify(buffer->Lock(&pixels,nullptr,nullptr));
                for(int i=0;i<64*64;++i){pixels[i*4]=20;pixels[i*4+1]=40;pixels[i*4+2]=frame<30?220:50;pixels[i*4+3]=255;}verify(buffer->Unlock());verify(buffer->SetCurrentLength(64*64*4));
                ComPtr<IMFSample> sample;verify(MFCreateSample(&sample));verify(sample->AddBuffer(buffer.Get()));verify(sample->SetSampleTime(frame*10000000LL/30));verify(sample->SetSampleDuration(10000000LL/30));verify(writer->WriteSample(stream,sample.Get()));
            }verify(writer->Finalize());
        }
        {
            Video video;video.open(path);check(video.duration>1.9&&video.duration<2.1,"Duration incorrect");Image frame;
            check(video.frame(0,frame),"Initial frame absent");check(frame.width==64&&frame.height==64,"Dimensions incorrect");check(frame.pixels[0]>180,"RGB conversion incorrect");
            video.seek(1.4);check(video.frame(1.4,frame),"Seek frame absent");check(frame.pixels[0]<90,"Seek did not reach second half");
            video.seek(.2);check(video.frame(.2,frame),"Reverse seek frame absent");check(frame.pixels[0]>180,"Reverse seek incorrect");
            check(!video.frame(.201,frame),"Presented future frame early");
            for(double t=.24;t<2.2;t+=.04)video.frame(t,frame);
        }
        MFShutdown();CoUninitialize();std::cout<<"Media tests passed: WIC image import, H.264 encode/decode, RGBA conversion, duration, forward/backward seek, future-frame retention, EOF\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
