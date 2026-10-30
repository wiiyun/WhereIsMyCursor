#include "core.h"
#include "async_inverse.h"
namespace {
struct MagnifierApi {
    using Init=BOOL(WINAPI*)();using Show=BOOL(WINAPI*)(BOOL);
    HMODULE library=nullptr;Init init=nullptr,uninit=nullptr;Show show=nullptr;
    bool load(){
        if(library)return init&&uninit&&show;
        library=LoadLibraryExW(L"Magnification.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library)return false;
        init=reinterpret_cast<Init>(GetProcAddress(library,"MagInitialize"));uninit=reinterpret_cast<Init>(GetProcAddress(library,"MagUninitialize"));show=reinterpret_cast<Show>(GetProcAddress(library,"MagShowSystemCursor"));return init&&uninit&&show;
    }
    ~MagnifierApi(){if(library)FreeLibrary(library);}
} magnifier;
}
bool cursorApiInitialize(){return magnifier.load()&&magnifier.init();}
bool cursorApiShow(BOOL show){return magnifier.load()&&magnifier.show(show);}
void cursorApiUninitialize(){if(magnifier.uninit)magnifier.uninit();}

static LRESULT CALLBACK overlayProc(HWND window,UINT message,WPARAM wp,LPARAM lp){if(message==WM_NCHITTEST)return HTTRANSPARENT;if(message==WM_MOUSEACTIVATE)return MA_NOACTIVATE;return DefWindowProcW(window,message,wp,lp);}
Overlay::Overlay(HINSTANCE instance){
    WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=overlayProc;cls.hInstance=instance;cls.lpszClassName=L"WhereIsMyCursor.NativeOverlay";
    if(!RegisterClassExW(&cls))require(GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"Cannot register overlay");
    window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,cls.lpszClassName,L"WhereIsMyCursor",WS_POPUP,0,0,1,1,nullptr,nullptr,instance,nullptr);require(window,"Cannot create overlay");
    excluded=SetWindowDisplayAffinity(window,0x11)!=FALSE;
}
Overlay::~Overlay(){hide();worker.reset();if(window)DestroyWindow(window);}
void Overlay::hide(){if(shown||displayed||wanted){if(worker)worker->cancel();ShowWindow(window,SW_HIDE);output.reset();wanted.reset();displayed.reset();planFullWidth=planFullHeight=0;}shown=false;}
void Overlay::read(HCURSOR cursor){
    ICONINFO icon{};require(GetIconInfo(cursor,&icon),"Cannot query cursor");
    try{
        BITMAP bm{};require(GetObjectW(icon.hbmColor?icon.hbmColor:icon.hbmMask,sizeof(bm),&bm),"Cannot query cursor bitmap");
        width=std::clamp(int(bm.bmWidth),1,512);height=std::clamp(int(icon.hbmColor?bm.bmHeight:bm.bmHeight/2),1,512);hotX=int(icon.xHotspot);hotY=int(icon.yHotspot);
        Dib black,white;black.ensure(width,height);white.ensure(width,height);black.fill(0xFF000000);white.fill(0xFFFFFFFF);
        require(DrawIconEx(black.dc,0,0,cursor,width,height,0,nullptr,DI_NORMAL)&&DrawIconEx(white.dc,0,0,cursor,width,height,0,nullptr,DI_NORMAL),"Cannot read cursor image");GdiFlush();
        raster.resize(size_t(width)*height);inversion=false;
        for(size_t i=0;i<raster.size();i++){
            uint32_t b=black.pixels[i],w=white.pixels[i];Pixel p{};int difference=-255;bool invert=false;
            for(int shift:{0,8,16}){int bc=(b>>shift)&255,wc=(w>>shift)&255;difference=std::max(difference,wc-bc);if(bc>wc+32)invert=true;}
            if(invert){p.inverted=true;p.andMask=(w^b)&0xFFFFFF;p.xorMask=b&0xFFFFFF;inversion=true;}
            else{uint32_t alpha=255-std::clamp(difference,0,255);p.color=alpha<<24;for(int shift:{0,8,16})p.color|=std::min((b>>shift)&255,alpha)<<shift;}
            raster[i]=p;
        }
        content={width,height,0,0};
        for(int y=0;y<height;y++)for(int x=0;x<width;x++){
            const auto& p=raster[size_t(y)*width+x];if(p.inverted||(p.color>>24)){
                content.left=std::min(content.left,LONG(x));content.top=std::min(content.top,LONG(y));content.right=std::max(content.right,LONG(x+1));content.bottom=std::max(content.bottom,LONG(y+1));
            }
        }
        if(content.right<=content.left||content.bottom<=content.top)content={0,0,1,1};
        cached=cursor;++generation;wanted.reset();planFullWidth=planFullHeight=0;if(worker)worker->cancel();
    }catch(...){if(icon.hbmMask)DeleteObject(icon.hbmMask);if(icon.hbmColor)DeleteObject(icon.hbmColor);throw;}
    if(icon.hbmMask)DeleteObject(icon.hbmMask);if(icon.hbmColor)DeleteObject(icon.hbmColor);
}
std::shared_ptr<const RenderPlan> Overlay::makePlan(double scale){
    int fw=std::max(1,int(width*scale)),fh=std::max(1,int(height*scale)),hx=int(hotX*scale),hy=int(hotY*scale);
    if(wanted&&fw==planFullWidth&&fh==planFullHeight&&hx==planHotX&&hy==planHotY)return wanted;
    // Crop in destination coordinates, preserving the exact old nearest-neighbor
    // mapping and hotspot even at non-integral animation scales.
    int left=(content.left*fw+width-1)/width,top=(content.top*fh+height-1)/height;
    int right=(content.right*fw+width-1)/width,bottom=(content.bottom*fh+height-1)/height;
    auto plan=std::make_shared<RenderPlan>();plan->generation=generation;plan->width=std::max(1,right-left);plan->height=std::max(1,bottom-top);plan->offsetX=left-hx;plan->offsetY=top-hy;
    require(uint64_t(plan->width)*plan->height<=16777216,"Invalid cursor size");plan->base.resize(size_t(plan->width)*plan->height);
    RECT bounds{plan->width,plan->height,0,0};
    for(int y=0;y<plan->height;y++){
        for(int x=0;x<plan->width;){
            const auto& p=raster[size_t((y+top)*height/fh)*width+(x+left)*width/fw];
            if(!p.inverted){plan->base[size_t(y)*plan->width+x]=p.color;++x;continue;}
            int begin=x;do{
                plan->base[size_t(y)*plan->width+x]=0;++x;
                if(x==plan->width)break;
                const auto& next=raster[size_t((y+top)*height/fh)*width+(x+left)*width/fw];
                if(!next.inverted||next.andMask!=p.andMask||next.xorMask!=p.xorMask)break;
            }while(true);
            plan->spans.push_back({y,begin,x-begin,p.andMask,p.xorMask});bounds.left=std::min(bounds.left,LONG(begin));bounds.right=std::max(bounds.right,LONG(x));bounds.top=std::min(bounds.top,LONG(y));bounds.bottom=y+1;
        }
    }
    plan->inverseBounds=bounds;planFullWidth=fw;planFullHeight=fh;planHotX=hx;planHotY=hy;wanted=std::move(plan);return wanted;
}
void Overlay::render(const CURSORINFO& cursor,double scale,u64 now){
    if(animated!=cursor.hCursor){animated=cursor.hCursor;step=0;frameDue=0;cached=nullptr;}
    HCURSOR frame=cursor.hCursor;
    using GetFrame=HCURSOR(WINAPI*)(HCURSOR,DWORD,DWORD,DWORD*,DWORD*);
    static auto getFrame=reinterpret_cast<GetFrame>(GetProcAddress(GetModuleHandleW(L"user32.dll"),"GetCursorFrameInfo"));
    if(getFrame){DWORD rate=0,steps=0;HCURSOR candidate=getFrame(frame,0,step,&rate,&steps);if(steps>1){if(now>=frameDue){step=(step+1)%steps;frameDue=now+std::max(DWORD(1),rate)*1000/60;candidate=getFrame(frame,0,step,&rate,&steps);}if(candidate)frame=candidate;}}
    if(frame!=cached)read(frame);
    auto plan=makePlan(scale);
    auto present=[&](const RenderPlan& ready,HDC dc){
        POINT position{cursor.ptScreenPos.x+ready.offsetX,cursor.ptScreenPos.y+ready.offsetY},origin{};SIZE size{ready.width,ready.height};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        require(UpdateLayeredWindow(window,nullptr,&position,&size,dc,&origin,0,&blend,ULW_ALPHA),"Cannot update layered cursor");
    };
    if(inversion){
        require(excluded,"Capture exclusion unavailable; inverse rendering requires Windows 10 2004+");
        if(!worker)worker=std::make_unique<AsyncInverse>();
        auto completed=worker->take();
        if(completed){
            if(completed->plan->generation==generation){present(*completed->plan,completed->pixels.dc);displayed=completed->plan;}
            worker->recycle(std::move(completed));
        }
        worker->submit(plan,POINT{cursor.ptScreenPos.x+plan->offsetX,cursor.ptScreenPos.y+plan->offsetY});
    }
    else if(displayed!=plan){
        output.ensure(plan->width,plan->height);std::copy(plan->base.begin(),plan->base.end(),output.pixels);present(*plan,output.dc);displayed=plan;
    }
    if(displayed){
        // Movement never waits for screen capture; colors catch up when the next
        // background frame is ready. There is only one pending capture job.
        SetWindowPos(window,HWND_TOPMOST,cursor.ptScreenPos.x+displayed->offsetX,cursor.ptScreenPos.y+displayed->offsetY,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);shown=true;
    }
}
static std::wstring readyName(DWORD pid){return L"Local\\WhereIsMyCursor.Guard."+std::to_wstring(pid);}
CursorVisibility::CursorVisibility(){
    require(cursorApiInitialize(),"Cannot initialize cursor visibility");initialized=true;
    auto eventName=readyName(GetCurrentProcessId());HANDLE ready=CreateEventW(nullptr,TRUE,FALSE,eventName.c_str());
    if(!ready){cursorApiUninitialize();initialized=false;throw std::runtime_error("Cannot create cursor guard event");}
    auto guardPath=exePath();guardPath.resize(guardPath.find_last_of(L"\\")+1);guardPath+=L"WhereIsMyCursor.Guard.exe";
    std::wstring command=L"\""+guardPath+L"\" "+std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{sizeof(startup)};PROCESS_INFORMATION process{};
    BOOL launched=CreateProcessW(guardPath.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process);
    bool ok=false;
    if(launched){HANDLE wait[]{ready,process.hProcess};ok=WaitForMultipleObjects(2,wait,FALSE,5000)==WAIT_OBJECT_0;CloseHandle(process.hThread);CloseHandle(process.hProcess);}
    CloseHandle(ready);if(!ok){cursorApiUninitialize();initialized=false;throw std::runtime_error("Cursor recovery guard failed to start");}
}
CursorVisibility::~CursorVisibility(){restore();if(initialized)cursorApiUninitialize();}
void CursorVisibility::hide(){if(!hidden){require(cursorApiShow(FALSE),"Cannot hide native cursor");hidden=true;}}
void CursorVisibility::restore(){if(hidden){cursorApiShow(TRUE);hidden=false;}}
