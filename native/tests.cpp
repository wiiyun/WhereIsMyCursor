#include "core.h"
#include <cstdio>

static std::wstring folder(){auto path=exePath();path.resize(path.find_last_of(L"\\")+1);return path;}
static void report(const std::string& s){auto path=folder()+L"native-test-result.txt";HANDLE f=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);if(f!=INVALID_HANDLE_VALUE){DWORD n;WriteFile(f,s.data(),DWORD(s.size()),&n,nullptr);CloseHandle(f);}}
static LRESULT CALLBACK backgroundProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_PAINT){PAINTSTRUCT paint{};HDC dc=BeginPaint(window,&paint);RECT r{};GetClientRect(window,&r);HBRUSH brush=CreateSolidBrush(COLORREF(GetWindowLongPtrW(window,GWLP_USERDATA)));FillRect(dc,&r,brush);DeleteObject(brush);EndPaint(window,&paint);return 0;}return DefWindowProcW(window,message,wp,lp);
}
static void settle(){MSG message{};while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}DwmFlush();Sleep(60);DwmFlush();}
static uint32_t capturePixel(int x,int y){Dib pixel;pixel.ensure(1,1);HDC desktop=GetDC(nullptr);BOOL ok=BitBlt(pixel.dc,0,0,1,1,desktop,x,y,SRCCOPY|CAPTUREBLT);GdiFlush();ReleaseDC(nullptr,desktop);require(ok,"Test capture failed");return pixel.pixels[0]&0xFFFFFF;}
static void drawAndSettle(Overlay& overlay,const CURSORINFO& cursor,double scale){
    u64 until=GetTickCount64()+160;
    do{overlay.render(cursor,scale,GetTickCount64());Sleep(4);}while(GetTickCount64()<until);
    settle();
}
static void integration(){
    HINSTANCE instance=GetModuleHandleW(nullptr);WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=backgroundProc;cls.hInstance=instance;cls.lpszClassName=L"WhereIsMyCursor.TestBackground";RegisterClassExW(&cls);
    HWND background=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,L"Native rendering test",WS_POPUP,200,200,160,160,nullptr,nullptr,instance,nullptr);require(background,"Cannot create test background");SetWindowLongPtrW(background,GWLP_USERDATA,RGB(20,120,200));ShowWindow(background,SW_SHOWNOACTIVATE);UpdateWindow(background);settle();
    std::vector<unsigned char> mask(64);std::fill_n(mask.data(),32,255);for(int y=4;y<12;y++)mask[32+y*2+1]=128;
    HBITMAP bitmap=CreateBitmap(16,32,1,1,mask.data());ICONINFO info{};info.hbmMask=bitmap;HCURSOR cursor=static_cast<HCURSOR>(CreateIconIndirect(&info));DeleteObject(bitmap);require(cursor,"Cannot create XOR cursor");
    try{
        Overlay overlay(instance);CURSORINFO current{sizeof(current)};current.hCursor=cursor;current.ptScreenPos={240,240};current.flags=CURSOR_SHOWING;
        drawAndSettle(overlay,current,3);
        require(capturePixel(264,252)==0x1478C8,"Overlay captured itself");
        SetWindowDisplayAffinity(overlay.handle(),0);settle();
        require(capturePixel(264,252)==0xEB8737,"Displayed cursor is not the inverse of the background");
        require(capturePixel(240,240)==0x1478C8,"Transparent pixels changed the background or added an outline");
        SetWindowDisplayAffinity(overlay.handle(),0x11);settle();
        SetWindowLongPtrW(background,GWLP_USERDATA,RGB(250,30,10));InvalidateRect(background,nullptr,TRUE);UpdateWindow(background);settle();
        drawAndSettle(overlay,current,3);SetWindowDisplayAffinity(overlay.handle(),0);settle();require(capturePixel(264,252)==0x05E1F5,"Inverse cursor did not follow a changing background");
        SetWindowDisplayAffinity(overlay.handle(),0x11);settle();DWORD before=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
        for(int i=0;i<80;i++){current.ptScreenPos.x=240+i%4;overlay.render(current,2+(i%8)*.1,GetTickCount64());}
        DWORD after=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);require(after<=before+12,"GDI resources leaked during resizing");
        current.ptScreenPos={80,120};drawAndSettle(overlay,current,20);
        RECT maximum{};GetWindowRect(overlay.handle(),&maximum);require(maximum.left==240&&maximum.top==200&&maximum.right-maximum.left==20&&maximum.bottom-maximum.top==160,"Maximum-scale crop or hotspot is incorrect");
        SetWindowDisplayAffinity(overlay.handle(),0);settle();require(capturePixel(250,240)==0x05E1F5&&capturePixel(230,240)==0xFA1E0A,"Maximum-scale inversion or transparency failed");SetWindowDisplayAffinity(overlay.handle(),0x11);settle();
        current.ptScreenPos={240,240};drawAndSettle(overlay,current,2.1);RECT fractional{};GetWindowRect(overlay.handle(),&fractional);
        require(fractional.left==257&&fractional.top==249&&fractional.right-fractional.left==2&&fractional.bottom-fractional.top==16,"Fractional-scale crop moved the hotspot or changed nearest-neighbor coverage");
        overlay.hide();Sleep(60);require(!overlay.visible(),"A cancelled capture made the hidden cursor visible again");
        current.hCursor=LoadCursorW(nullptr,IDC_ARROW);drawAndSettle(overlay,current,8);
        RECT arrowBefore{};GetWindowRect(overlay.handle(),&arrowBefore);Sleep(40);overlay.render(current,8,GetTickCount64());RECT arrowAfter{};GetWindowRect(overlay.handle(),&arrowAfter);
        require(EqualRect(&arrowBefore,&arrowAfter),"Stale inverse work replaced a normal cursor");overlay.hide();
        // Exercise actual theme cursors and live inversion masks without substitutions.
        for(LPCWSTR id:{IDC_ARROW,IDC_IBEAM,IDC_HAND,IDC_SIZEALL}){current.hCursor=LoadCursorW(nullptr,id);drawAndSettle(overlay,current,8);require(overlay.visible(),"Native cursor render failed");overlay.hide();}
    }catch(...){DestroyCursor(cursor);DestroyWindow(background);throw;}
    DestroyCursor(cursor);DestroyWindow(background);uiTest(instance);
}
int selfTest(bool testIntegration){
    try{
        ShakeDetector detector;bool triggered=false;constexpr double pi=3.141592653589793;
        auto trajectory=[&](double amplitude,double period,double sensitivity,double dpiScale=1.){
            detector.reset();bool result=false;
            for(int i=0;i<90;i++){
                double physical=(300+amplitude*std::sin(i*16*2*pi/period))*dpiScale;
                result|=detector.add(i*16,physical/dpiScale,100,sensitivity);
            }
            return result;
        };
        for(int i=0;i<60;i++)triggered|=detector.add(i*16,i*20,100,.5);require(!triggered,"Straight movement triggered");
        require(!trajectory(12,128,.5),"Small jitter triggered");
        require(!trajectory(70,128,.5),"Routine short corrections triggered");
        require(!trajectory(150,960,.5),"Slow large movement triggered");
        require(trajectory(150,128,.5),"Deliberate rapid large shake did not trigger");
        require(!trajectory(75,128,.5),"Moderate shake unexpectedly met the old threshold");
        require(trajectory(75,128,Settings::DefaultSensitivity),"Adjusted default did not accept moderate deliberate shaking");
        require(!trajectory(12,128,Settings::DefaultSensitivity),"Adjusted default accepted small jitter");
        require(!trajectory(150,128,.25),"Lowest sensitivity did not reject insufficient travel");
        for(double dpi:{1.,1.5,2.}){
            require(trajectory(150,128,.5,dpi),"DPI-normalized deliberate shake failed");
            require(!trajectory(70,128,.5,dpi),"DPI-normalized small motion triggered");
        }
        // Four reversals in a short burst are insufficient; require five.
        detector.reset();triggered=false;
        for(int i=0;i<=20;i++){int stroke=i/4,phase=i%4;double x=stroke%2?300-phase*75:phase*75;triggered|=detector.add(i*16,x,100,2);}
        require(!triggered,"Four reversals incorrectly triggered");
        for(int i=21;i<=24;i++){int stroke=i/4,phase=i%4;double x=stroke%2?300-phase*75:phase*75;triggered|=detector.add(i*16,x,100,2);}
        require(triggered,"Fifth substantial reversal failed to trigger");
        Animation animation;double lastPosition=0;
        for(int t=0;t<=2200;t+=4){lastPosition=300+150*std::sin(t*2*pi/128);animation.motion(t,lastPosition,100);if(t==300)animation.trigger(t,8);}
        require(animation.active(2200)&&animation.value(2200)>6&&animation.value(2200)<=8,"Continuing to fling must keep the pointer enlarged");
        double prior=animation.value(2200);animation.trigger(2200,8);require(animation.value(2200)==prior,"A new fling caused a size jump");
        for(int t=2204;t<=2300;t+=4)animation.motion(t,lastPosition,100);
        require(animation.value(2300)<prior,"Stopping the fling did not promptly start shrinking");
        for(int t=2304;t<=2800;t+=4)animation.motion(t,lastPosition,100);
        require(!animation.active(2800)&&animation.value(2800)==1,"Stopped flinging did not return to the native size");
        auto flungSize=[&](double amplitude,double period){
            Animation a;for(int t=0;t<=1200;t+=4){a.motion(t,300+amplitude*std::sin(t*2*pi/period),100);if(t==300)a.trigger(t,8);}return a.value(1200);
        };
        require(flungSize(150,128)>flungSize(50,200)+1,"Stronger flinging should drive a larger pointer");
        animation.reset();
        for(int t=0;t<=600;t+=4){lastPosition=300+150*std::sin(t*2*pi/128);animation.motion(t,lastPosition,100);if(t==300)animation.trigger(t,8);}
        double straightStart=lastPosition;
        for(int t=604;t<=1400;t+=4)animation.motion(t,straightStart+(t-600)*2,100);
        require(!animation.active(1400),"Fast straight movement kept the pointer enlarged without flinging");
        auto holdCheck=[&](double hold){
            Animation a;double position=0;
            for(int t=0;t<=1000;t+=4){position=300+150*std::sin(t*2*pi/128);a.motion(t,position,100,hold);if(t==300)a.trigger(t,8);}
            for(int t=1004;t<=1040;t+=4)a.motion(t,position,100,hold);
            double stopped=a.value(1040);
            for(int t=1044;t<=1200;t+=4)a.motion(t,position,100,hold);
            if(hold>0)require(a.value(1200)==stopped,"Pointer shrank during the configured hold");
            else require(a.value(1200)<stopped,"Zero hold did not start shrinking promptly");
            for(int t=1204;t<=2000+int(hold*1000);t+=4)a.motion(t,position,100,hold);
            require(!a.active(4000)&&a.value(4000)==1,"Hold did not end with a complete shrink");
        };
        for(double hold:{0.,Settings::DefaultHoldSeconds,2.})holdCheck(hold);
        Animation resume;double position=0;
        for(int t=0;t<=1000;t+=4){position=300+150*std::sin(t*2*pi/128);resume.motion(t,position,100,.35);if(t==300)resume.trigger(t,8);}
        for(int t=1004;t<=1150;t+=4)resume.motion(t,position,100,.35);
        double held=resume.value(1150);resume.trigger(1150,8);require(resume.value(1150)==held,"Resuming during hold caused a jump");
        Settings s;s.scale=13.5;s.sensitivity=.65;s.holdSeconds=1.25;s.fullscreen=false;s.excluded=L"game.exe,\n苹果.exe, quote\"name.exe";auto temp=folder()+L"native-test-settings.json";s.save(temp);Settings read;read.load(temp);require(read.scale==s.scale&&read.sensitivity==s.sensitivity&&read.holdSeconds==s.holdSeconds&&read.fullscreen==s.fullscreen&&read.excluded==s.excluded,"JSON settings migration failed");DeleteFileW(temp.c_str());
        if(testIntegration)integration();
        report(testIntegration?"PASS: conservative shake detection, DPI-normalized jitter filtering, motion-driven spring growth, stop-to-shrink, stronger fling response and continuity, configurable hold and zero-hold recovery, Unicode JSON round-trip; actual on-screen XOR colors, transparency, changing background, capture exclusion, maximum/fractional scale crop and hotspot, asynchronous capture cancellation, GDI resource stability, arrow/I-beam/hand/resize; settings controls, numeric validation and reset; native guard startup.\r\n":"PASS: conservative shake detection, DPI-normalized jitter filtering, motion-driven spring growth, stop-to-shrink, stronger fling response and continuity, configurable hold and zero-hold recovery, Unicode JSON round-trip.\r\n");return 0;
    }catch(const std::exception& ex){report(std::string("FAIL: ")+ex.what()+"\r\n");return 1;}
}

int renderBenchmark(){
    try{
        HINSTANCE instance=GetModuleHandleW(nullptr);WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=backgroundProc;cls.hInstance=instance;cls.lpszClassName=L"WhereIsMyCursor.TestBackground";RegisterClassExW(&cls);
        HWND background=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,cls.lpszClassName,L"Cursor performance test",WS_POPUP,180,180,950,950,nullptr,nullptr,instance,nullptr);require(background,"Cannot create benchmark window");SetWindowLongPtrW(background,GWLP_USERDATA,RGB(20,120,200));ShowWindow(background,SW_SHOWNOACTIVATE);UpdateWindow(background);settle();
        std::vector<unsigned char> mask(256);std::fill_n(mask.data(),128,255);
        auto bit=[&](int x,int y){mask[128+y*4+x/8]|=static_cast<unsigned char>(128>>(x%8));};
        for(int y=2;y<30;y++)bit(15,y);for(int x=12;x<=18;x++){bit(x,2);bit(x,29);}
        HBITMAP bitmap=CreateBitmap(32,64,1,1,mask.data());ICONINFO info{};info.hbmMask=bitmap;HCURSOR cursor=static_cast<HCURSOR>(CreateIconIndirect(&info));DeleteObject(bitmap);
        std::string csv="scale,avg_ui_render_ms,p95_ui_render_ms,max_ui_render_ms\r\n";
        {
            Overlay overlay(instance);CURSORINFO current{sizeof(current)};current.hCursor=cursor;current.flags=CURSOR_SHOWING;
            LARGE_INTEGER frequency{};QueryPerformanceFrequency(&frequency);
            for(double scale:{4.,8.,20.}){
                std::vector<double> times;
                for(int i=0;i<56;i++){
                    current.ptScreenPos={400+i%25,300+i%15};LARGE_INTEGER begin{},end{};QueryPerformanceCounter(&begin);overlay.render(current,scale,GetTickCount64()+u64(i)*10);QueryPerformanceCounter(&end);
                    if(i>=8)times.push_back(double(end.QuadPart-begin.QuadPart)*1000/frequency.QuadPart);Sleep(4);
                }
                std::sort(times.begin(),times.end());double average=0;for(auto value:times)average+=value;average/=times.size();char row[160];snprintf(row,sizeof(row),"%.0f,%.4f,%.4f,%.4f\r\n",scale,average,times[size_t(times.size()*.95)],times.back());csv+=row;
            }
        }
        DestroyCursor(cursor);DestroyWindow(background);auto path=folder()+L"render-benchmark.csv";HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);require(file!=INVALID_HANDLE_VALUE,"Cannot write benchmark");DWORD count=0;WriteFile(file,csv.data(),DWORD(csv.size()),&count,nullptr);CloseHandle(file);return 0;
    }catch(const std::exception& ex){report(std::string("FAIL benchmark: ")+ex.what());return 1;}
}
