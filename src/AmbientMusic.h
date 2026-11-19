#pragma once
#include <mmsystem.h>
#include <filesystem>
#include <string>
#include <random>
#include <algorithm>
// Local ambient playback is independent of media/export clocks.
// Exposes play/pause/resume plus volume; the track repeats forever.
// Startup enters at a random point and fades in; resume never fades.
class AmbientMusic {
    bool opened_{},playing_{},started_{},fading_{};
    int volume_{70};
    float fadeSeconds_{2.8f};
    ULONGLONG lastCheck_{},fadeStart_{},lastFade_{};
    bool command(const wchar_t* text){
        auto error=mciSendStringW(text,nullptr,0,nullptr);
        if(!error)return true;
        wchar_t message[256]{};mciGetErrorStringW(error,message,256);
        errorText="Music playback unavailable";return false;
    }
    // MCI audio volume runs 0..1000; silent failures are ignored on codecs
    // that do not implement the audio command set.
    void applyLevel(int percent){
        if(!opened_)return;
        wchar_t text[64]{};
        swprintf_s(text,L"setaudio flexarAmbient volume to %d",std::clamp(percent,0,100)*10);
        mciSendStringW(text,nullptr,0,nullptr);
    }
    void applyVolume(){applyLevel(volume_);}
    long long trackLength(){
        wchar_t text[32]{};
        if(mciSendStringW(L"status flexarAmbient length",text,32,nullptr))return 0;
        return wcstoll(text,nullptr,10);
    }
public:
    std::string errorText;
    ~AmbientMusic(){if(opened_)mciSendStringW(L"close flexarAmbient",nullptr,0,nullptr);}
    bool playing()const{return playing_;}
    bool available()const{return opened_;}
    int volume()const{return volume_;}
    void setVolume(int percent){
        int next=std::clamp(percent,0,100);
        if(next==volume_)return;
        volume_=next;
        if(!fading_)applyVolume();
    }
    void open(const std::filesystem::path& path){
        errorText.clear();
        if(opened_){mciSendStringW(L"close flexarAmbient",nullptr,0,nullptr);opened_=false;}
        started_=playing_=fading_=false;
        if(!std::filesystem::exists(path)){errorText="Music file not found";return;}
        auto text=L"open \""+path.wstring()+L"\" type mpegvideo alias flexarAmbient";
        opened_=command(text.c_str());
        if(opened_)applyVolume();
    }
    void start(){
        if(!opened_)return;
        if(command(L"play flexarAmbient from 0 repeat")){
            playing_=started_=true;fading_=false;lastCheck_=GetTickCount64();
            applyVolume();
        }
    }
    // Startup only: enter at a random point and fade the volume up.
    void startRandom(){
        if(!opened_)return;
        long long length=trackLength(),from=0;
        if(length>0){
            std::mt19937 rng(std::random_device{}());
            long long low=length/10,high=std::max(low,length*3/4);
            from=std::uniform_int_distribution<long long>(low,high)(rng);
        }
        applyLevel(0);
        wchar_t text[96]{};
        swprintf_s(text,L"play flexarAmbient from %lld repeat",from);
        if(command(text)){
            playing_=started_=fading_=true;
            fadeStart_=lastFade_=lastCheck_=GetTickCount64();
        }else{
            fading_=false;playing_=false;applyVolume();
        }
    }
    void pause(){
        if(!opened_||!playing_)return;
        if(command(L"pause flexarAmbient")){
            playing_=false;fading_=false;
            // Startup fade is done; resume should sit at the chosen level.
            applyVolume();
        }
    }
    void resume(){
        if(!opened_)return;
        if(!started_){start();return;}
        if(command(L"play flexarAmbient")){
            playing_=true;fading_=false;
            applyVolume();
        }
    }
    void toggle(){if(playing_)pause();else resume();}
    void tick(){
        auto now=GetTickCount64();
        if(fading_){
            if(!playing_){
                fading_=false;applyVolume();
            }else{
                float t=float(now-fadeStart_)/(fadeSeconds_*1000.f);
                if(t>=1.f){fading_=false;applyVolume();}
                else if(now-lastFade_>=40){
                    lastFade_=now;
                    applyLevel(static_cast<int>(float(volume_)*t));
                }
            }
        }
        if(!playing_||now-lastCheck_<250)return;
        lastCheck_=now;
        wchar_t mode[32]{};
        if(mciSendStringW(L"status flexarAmbient mode",mode,32,nullptr)){playing_=false;fading_=false;errorText="Music status unavailable";return;}
        // "repeat" handles normal wraps; a stopped device means the transport was
        // interrupted, so restart it to keep the loop alive.
        if(wcscmp(mode,L"stopped")==0&&!command(L"play flexarAmbient from 0 repeat")){playing_=false;fading_=false;}
    }
};
