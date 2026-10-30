#pragma once
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <deque>
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>

using u64=ULONGLONG;
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Settings {
    static constexpr double DefaultSensitivity=.8;
    static constexpr double DefaultHoldSeconds=.35;
    double scale=8,sensitivity=DefaultSensitivity,holdSeconds=DefaultHoldSeconds;bool fullscreen=true;std::wstring excluded;
    void load(const std::wstring& path=L"");void save(const std::wstring& path=L"")const;
};
std::wstring exePath();
std::wstring settingsPath();
bool startupEnabled();
void setStartup(bool enabled);
struct Sample{u64 time;double x,y;};
class ShakeDetector {
    std::deque<Sample> points;
    double lastX=0,lastY=0;bool previous=false;
public:
    void reset(){points.clear();previous=false;}
    bool add(u64 now,double x,double y,double sensitivity);
};
class Animation {
    bool running=false,sampled=false,hasDirection=false;
    u64 sampledAt=0,lastFast=0,lastTurn=0,lastDriven=0;
    double x=0,y=0,dirX=0,dirY=0,stroke=0,filteredSpeed=0;
    double size=1,velocity=0,peak=8;
public:
    bool active(u64)const{return running;}
    double value(u64)const{return size;}
    void reset(){running=sampled=hasDirection=false;sampledAt=lastFast=lastTurn=lastDriven=0;x=y=dirX=dirY=stroke=filteredSpeed=velocity=0;size=1;}
    void trigger(u64 now,double scale){
        if(!running){size=1;velocity=0;}
        peak=std::clamp(scale,2.,20.);running=true;lastFast=lastTurn=lastDriven=now;
    }
    void motion(u64 now,double px,double py,double holdSeconds=0){
        if(!sampled){sampled=true;sampledAt=now;x=px;y=py;return;}
        double elapsed=double(now-sampledAt);if(elapsed<=0)return;
        double dx=px-x,dy=py-y,distance=std::hypot(dx,dy),speed=distance*1000/elapsed;
        x=px;y=py;sampledAt=now;
        filteredSpeed+=(speed-filteredSpeed)*(1-std::exp(-elapsed/35.));
        if(distance>.25){
            stroke+=distance;
            if(hasDirection&&dx*dirX+dy*dirY<-.35*distance*std::hypot(dirX,dirY)&&stroke>=14){lastTurn=now;stroke=0;}
            dirX=dx;dirY=dy;hasDirection=true;
        }
        if(speed>450)lastFast=now;
        if(!running)return;
        if(elapsed>500){size=1;velocity=0;running=false;return;}
        // Distinguish hardware sample gaps from the configurable post-shake hold.
        bool driving=now-lastFast<=28 && now-lastTurn<=140;
        if(driving)lastDriven=now;
        else if(now-lastDriven<std::clamp(holdSeconds,0.,2.)*1000){velocity=0;return;}
        double strength=.4+.6*std::clamp((filteredSpeed-650)/2400.,0.,1.);
        double target=driving?1+(peak-1)*strength:1;
        double remaining=std::min(elapsed/1000.,.064);
        while(remaining>0){
            double dt=std::min(remaining,.004);remaining-=dt;
            double omega=target>size?34.:24.,damping=target>size?.85:.95;
            velocity+=(omega*omega*(target-size)-2*damping*omega*velocity)*dt;
            size+=velocity*dt;
            if(size<1){size=1;velocity=std::max(0.,velocity);}
            if(size>peak){size=peak;velocity=std::min(0.,velocity);}
        }
        if(!driving&&size<1.008&&std::abs(velocity)<.15){size=1;velocity=0;running=false;}
    }
};
class Dib {
    HGDIOBJ previous=nullptr;
public:
    HDC dc=nullptr;HBITMAP bitmap=nullptr;uint32_t* pixels=nullptr;int width=0,height=0;
    Dib()=default;Dib(const Dib&)=delete;Dib& operator=(const Dib&)=delete;
    ~Dib(){reset();}
    void reset(){
        if(dc){if(previous)SelectObject(dc,previous);DeleteDC(dc);}
        if(bitmap)DeleteObject(bitmap);
        dc=nullptr;bitmap=nullptr;pixels=nullptr;previous=nullptr;width=height=0;
    }
    void ensure(int w,int h){
        if(width==w && height==h)return;
        require(w>0 && h>0 && uint64_t(w)*h<=16777216,"Invalid cursor surface size");reset();
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(nullptr,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        dc=CreateCompatibleDC(nullptr);if(!bitmap||!dc){reset();throw std::runtime_error("CreateDIBSection failed");}previous=SelectObject(dc,bitmap);width=w;height=h;
    }
    void fill(uint32_t color){std::fill_n(pixels,size_t(width)*height,color);}
};
struct Pixel {
    uint32_t color=0,andMask=0,xorMask=0;bool inverted=false;
};
class AsyncInverse;
struct RenderPlan;
class Overlay {
    HWND window=nullptr;Dib output;std::vector<Pixel> raster;
    std::unique_ptr<AsyncInverse> worker;
    std::shared_ptr<const RenderPlan> wanted,displayed;
    HCURSOR cached=nullptr,animated=nullptr;int width=0,height=0,hotX=0,hotY=0;
    RECT content{};int planFullWidth=0,planFullHeight=0,planHotX=0,planHotY=0;
    UINT step=0;u64 frameDue=0,generation=0;bool inversion=false,excluded=false,shown=false;
    void read(HCURSOR cursor);
    std::shared_ptr<const RenderPlan> makePlan(double scale);
public:
    explicit Overlay(HINSTANCE instance);
    ~Overlay();
    void hide();bool visible()const{return shown;}
    void render(const CURSORINFO& cursor,double scale,u64 now);
    HWND handle()const{return window;}
};
class CursorVisibility {
    bool initialized=false,hidden=false;
public:
    CursorVisibility();~CursorVisibility();
    void hide();void restore();

};
bool shouldBlock(const Settings& settings);
int selfTest(bool integration);

void uiTest(HINSTANCE instance);




bool cursorApiInitialize();bool cursorApiShow(BOOL show);void cursorApiUninitialize();


int renderBenchmark();
