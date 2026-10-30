#include "core.h"
#include "language.h"
#include "build/version.h"
#include <memory>

static constexpr UINT trayMessage=WM_APP+1;
static constexpr int idEnable=1,idSettings=2,idQuit=3;
static constexpr int scaleSlider=110,scaleEdit=111,scaleReset=112,sensSlider=120,sensEdit=121,sensReset=122;
static constexpr int holdSlider=150,holdEdit=151,holdReset=152;
static constexpr int scaleHelp=160,sensHelp=161,holdHelp=162;
static constexpr int fullscreenCheck=130,startupCheck=131,excludedEdit=140;
static LRESULT CALLBACK mainProc(HWND,UINT,WPARAM,LPARAM);
static LRESULT CALLBACK settingsProc(HWND,UINT,WPARAM,LPARAM);
static LRESULT CALLBACK sliderProc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR){
    if(message==WM_MOUSEWHEEL)wp=MAKEWPARAM(LOWORD(wp),-GET_WHEEL_DELTA_WPARAM(wp));
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,sliderProc,id);
    return DefSubclassProc(window,message,wp,lp);
}
static std::wstring text(HWND window){int count=GetWindowTextLengthW(window);std::wstring s(count+1,0);GetWindowTextW(window,s.data(),count+1);s.resize(count);return s;}
static std::wstring errorText(const std::exception& ex){std::string s=ex.what();int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);std::wstring result(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),result.data(),n);return result;}
struct ControlRect{HWND window;int x,y,w,h;};
class App {
public:
    HINSTANCE instance;HWND main=nullptr,settingsWindow=nullptr;Settings settings;
    Overlay overlay;CursorVisibility visibility;ShakeDetector detector;Animation animation;
    NOTIFYICONDATAW tray{};bool enabled=true,quitting=false,precise=false,blocked=false,reportedError=false;
    bool initializingSettings=false,updatingNumeric=false,settingsDirty=false;std::wstring settingsOverride;
    HMONITOR pointerMonitor=nullptr;double gestureScale=1;
    u64 lastGuard=0;UINT taskbarMessage=0,dpi=96;HFONT font=nullptr;std::vector<ControlRect> controls;
    bool chinese;
    const wchar_t* tr(const wchar_t* zh,const wchar_t* en)const{return uiText(chinese,zh,en);}
    explicit App(HINSTANCE h,LANGID language=GetUserDefaultUILanguage()):instance(h),overlay(h),chinese(chineseUI(language)){
        settings.load();
        WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=mainProc;cls.hInstance=h;cls.lpszClassName=L"WhereIsMyCursor.NativeMain";cls.hIcon=LoadIconW(h,MAKEINTRESOURCEW(101));RegisterClassExW(&cls);
        main=CreateWindowExW(0,cls.lpszClassName,L"WhereIsMyCursor",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,h,this);require(main,"Cannot create tray window");
        taskbarMessage=RegisterWindowMessageW(L"TaskbarCreated");tray.cbSize=sizeof(tray);tray.hWnd=main;tray.uID=1;tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;tray.uCallbackMessage=trayMessage;tray.hIcon=LoadIconW(h,MAKEINTRESOURCEW(101));wcscpy_s(tray.szTip,128,tr(L"WhereIsMyCursor " APP_VERSION_W L" · 晃动查找指针",L"WhereIsMyCursor " APP_VERSION_W L" · Shake to find your cursor"));addTray();
    }
    ~App(){stop();if(settingsDirty){try{settings.save(settingsOverride);}catch(...){}}if(settingsWindow)DestroyWindow(settingsWindow);if(font)DeleteObject(font);Shell_NotifyIconW(NIM_DELETE,&tray);if(main)DestroyWindow(main);}
    void addTray(){Shell_NotifyIconW(NIM_ADD,&tray);}
    void timing(bool active){if(precise==active)return;if(active)precise=timeBeginPeriod(1)==TIMERR_NOERROR;else{timeEndPeriod(1);precise=false;}}
    void stop(){animation.reset();detector.reset();visibility.restore();overlay.hide();timing(false);}
    void tick(u64 now){
        if(!enabled){stop();return;}CURSORINFO cursor{sizeof(cursor)};if(!GetCursorInfo(&cursor)||cursor.flags!=CURSOR_SHOWING){stop();return;}
        if(now-lastGuard>=80){blocked=shouldBlock(settings);lastGuard=now;}if(blocked){stop();return;}
        HMONITOR monitor=MonitorFromPoint(cursor.ptScreenPos,MONITOR_DEFAULTTONEAREST);
        if(monitor!=pointerMonitor){
            pointerMonitor=monitor;
            // The tray owner is hidden. Moving it gives us this monitor's DPI
            // through the per-monitor-aware GetDpiForWindow API, without capture
            // or a new helper window. Reset at screen boundaries to avoid jumps.
            SetWindowPos(main,nullptr,cursor.ptScreenPos.x,cursor.ptScreenPos.y,1,1,SWP_NOZORDER|SWP_NOACTIVATE);
            UINT actualDpi=GetDpiForWindow(main);gestureScale=std::max(1.,double(actualDpi?actualDpi:96)/96.);detector.reset();animation.reset();
        }
        animation.motion(now,cursor.ptScreenPos.x/gestureScale,cursor.ptScreenPos.y/gestureScale,settings.holdSeconds);
        if(detector.add(now,cursor.ptScreenPos.x/gestureScale,cursor.ptScreenPos.y/gestureScale,settings.sensitivity)){animation.trigger(now,settings.scale);detector.reset();}
        if(!animation.active(now)){visibility.restore();overlay.hide();timing(false);return;}
        timing(true);
        try{overlay.render(cursor,animation.value(now),now);if(overlay.visible())visibility.hide();else visibility.restore();}
        catch(const std::exception& ex){stop();if(!reportedError){reportedError=true;tray.uFlags=NIF_INFO;wcscpy_s(tray.szInfoTitle,64,tr(L"指针放大暂不可用",L"Cursor enlargement unavailable"));auto message=errorText(ex);wcsncpy_s(tray.szInfo,256,message.c_str(),_TRUNCATE);tray.dwInfoFlags=NIIF_WARNING;Shell_NotifyIconW(NIM_MODIFY,&tray);tray.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;}}
    }
    void menu(){
        POINT p{};GetCursorPos(&p);HMENU menu=CreatePopupMenu();AppendMenuW(menu,MF_STRING|(enabled?MF_CHECKED:0),idEnable,tr(L"启用晃动放大",L"Enable shake to enlarge"));AppendMenuW(menu,MF_STRING,idSettings,tr(L"设置…",L"Settings…"));AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,idQuit,tr(L"退出",L"Exit"));
        SetForegroundWindow(main);UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,main,nullptr);DestroyMenu(menu);PostMessageW(main,WM_NULL,0,0);if(command)SendMessageW(main,WM_COMMAND,command,0);
    }
    int px(int v)const{return MulDiv(v,dpi,96);}
    HWND control(const wchar_t* cls,const wchar_t* label,DWORD style,int id,int x,int y,int w,int h){
        HWND window=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,label,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),settingsWindow,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);require(window,"Cannot create settings control");
        if(wcscmp(cls,TRACKBAR_CLASSW)==0)require(SetWindowSubclass(window,sliderProc,1,0),"Cannot configure slider wheel direction");
        SendMessageW(window,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);controls.push_back({window,x,y,w,h});return window;
    }
    void fontForDpi(){if(font)DeleteObject(font);font=CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");}
    void resizeControls(){fontForDpi();for(auto c:controls){SetWindowPos(c.window,nullptr,px(c.x),px(c.y),px(c.w),px(c.h),SWP_NOZORDER|SWP_NOACTIVATE);SendMessageW(c.window,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);}}
    void setNumeric(int edit,double value){wchar_t buffer[32];swprintf(buffer,32,L"%.2f",value);std::wstring label=buffer;while(label.size()>1&&label.back()==L'0')label.pop_back();if(label.back()==L'.')label.pop_back();updatingNumeric=true;SetDlgItemTextW(settingsWindow,edit,label.c_str());updatingNumeric=false;}
    bool numeric(int id,double lo,double hi,double step,double& result,bool showError){
        auto s=text(GetDlgItem(settingsWindow,id));wchar_t* end=nullptr;double v=wcstod(s.c_str(),&end);while(end&&iswspace(*end))end++;
        if(end==s.c_str()||!end||*end||!std::isfinite(v)||v<lo||v>hi){if(showError){MessageBoxW(settingsWindow,id==scaleEdit?tr(L"放大倍数请输入 2 到 20 之间的数值。",L"Enter a scale between 2 and 20."):tr(L"灵敏度请输入 0.25 到 2 之间的数值。",L"Enter a sensitivity between 0.25 and 2."),tr(L"检查输入",L"Check input"),MB_OK|MB_ICONINFORMATION);SetFocus(GetDlgItem(settingsWindow,id));SendDlgItemMessageW(settingsWindow,id,EM_SETSEL,0,-1);}return false;}
        result=std::clamp(std::round(v/step)*step,lo,hi);return true;
    }
    void changed(){settingsDirty=true;lastGuard=0;detector.reset();SetTimer(settingsWindow,1,300,nullptr);}
    void numericChanged(int id){
        if(initializingSettings||updatingNumeric)return;
        double value;if(!numeric(id,id==scaleEdit?2:(id==holdEdit?0:.25),id==scaleEdit?20:2,id==scaleEdit?.5:.05,value,false))return;
        SendDlgItemMessageW(settingsWindow,id==scaleEdit?scaleSlider:(id==holdEdit?holdSlider:sensSlider),TBM_SETPOS,TRUE,LPARAM(std::lround(value/(id==scaleEdit?.5:.05))));
        double& current=id==scaleEdit?settings.scale:(id==holdEdit?settings.holdSeconds:settings.sensitivity);
        if(current!=value){current=value;changed();}
    }
    void editCommit(int id){numericChanged(id);setNumeric(id,id==scaleEdit?settings.scale:(id==holdEdit?settings.holdSeconds:settings.sensitivity));}
    bool persist(){
        KillTimer(settingsWindow,1);if(!settingsDirty)return true;
        try{settings.save(settingsOverride);settingsDirty=false;return true;}
        catch(const std::exception& ex){auto message=std::wstring(tr(L"保存失败：",L"Could not save settings: "))+errorText(ex);MessageBoxW(settingsWindow,message.c_str(),L"WhereIsMyCursor",MB_OK|MB_ICONERROR);return false;}
    }
    void closeSettings(){if(persist())DestroyWindow(settingsWindow);}
    void openSettings(){
        if(settingsWindow){ShowWindow(settingsWindow,SW_RESTORE);SetForegroundWindow(settingsWindow);return;}
        initializingSettings=true;dpi=GetDpiForSystem();fontForDpi();controls.clear();
        WNDCLASSEXW cls{sizeof(cls)};cls.lpfnWndProc=settingsProc;cls.hInstance=instance;cls.lpszClassName=L"WhereIsMyCursor.NativeSettings";cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));RegisterClassExW(&cls);
        RECT rect{0,0,px(440),px(530)};AdjustWindowRectExForDpi(&rect,WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,WS_EX_APPWINDOW,dpi);int w=rect.right-rect.left,h=rect.bottom-rect.top;
        POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&monitor);auto area=monitor.rcWork;
        settingsWindow=CreateWindowExW(WS_EX_APPWINDOW,cls.lpszClassName,tr(L"WhereIsMyCursor " APP_VERSION_W L" · 设置",L"WhereIsMyCursor " APP_VERSION_W L" · Settings"),WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,area.left+(area.right-area.left-w)/2,area.top+(area.bottom-area.top-h)/2,w,h,main,nullptr,instance,this);require(settingsWindow,"Cannot create settings window");
        control(L"STATIC",tr(L"晃动鼠标，找到当前指针",L"Shake your cursor to find it"),0,0,24,18,392,28);
        control(L"STATIC",tr(L"最大放大倍数",L"Maximum scale"),0,0,24,61,130,25);
        control(L"STATIC",tr(L"\u0032\u2013\u0032\u0030 \u500d\uff0c\u968f\u7529\u52a8\u5f3a\u5ea6\u53d8\u5316\u3002",L"2-20x; size follows shake strength."),SS_RIGHT,scaleHelp,164,61,252,25);
        auto scale=control(TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,scaleSlider,20,92,224,26);SendMessageW(scale,TBM_SETRANGE,TRUE,MAKELPARAM(4,40));SendMessageW(scale,TBM_SETPOS,TRUE,LPARAM(std::lround(settings.scale*2)));
        control(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,scaleEdit,254,90,76,28);control(L"BUTTON",tr(L"默认",L"Default"),WS_TABSTOP|BS_PUSHBUTTON,scaleReset,348,90,68,28);setNumeric(scaleEdit,settings.scale);
        control(L"STATIC",L"",SS_ETCHEDHORZ,0,24,130,392,2);
        control(L"STATIC",tr(L"晃动灵敏度",L"Shake sensitivity"),0,0,24,141,130,25);
        control(L"STATIC",tr(L"\u6570\u503c\u8d8a\u9ad8\u8d8a\u5bb9\u6613\u89e6\u53d1\u3002",L"Higher values trigger more easily."),SS_RIGHT,sensHelp,164,141,252,25);
        auto sens=control(TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,sensSlider,20,172,224,26);SendMessageW(sens,TBM_SETRANGE,TRUE,MAKELPARAM(5,40));SendMessageW(sens,TBM_SETPOS,TRUE,LPARAM(std::lround(settings.sensitivity*20)));
        control(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,sensEdit,254,170,76,28);control(L"BUTTON",tr(L"默认",L"Default"),WS_TABSTOP|BS_PUSHBUTTON,sensReset,348,170,68,28);setNumeric(sensEdit,settings.sensitivity);
        control(L"STATIC",L"",SS_ETCHEDHORZ,0,24,210,392,2);
        control(L"STATIC",tr(L"\u505c\u7559\u65f6\u95f4\uff08\u79d2\uff09",L"Hold duration (s)"),0,0,24,221,130,25);
        control(L"STATIC",tr(L"\u505c\u6b62\u540e\u4fdd\u6301 0\u20132 \u79d2\uff0c\u518d\u7f29\u5c0f\u3002",L"Wait 0-2 s after shaking, then shrink."),SS_RIGHT,holdHelp,164,221,252,25);
        auto hold=control(TRACKBAR_CLASSW,L"",WS_TABSTOP|TBS_HORZ|TBS_NOTICKS,holdSlider,20,252,224,26);SendMessageW(hold,TBM_SETRANGE,TRUE,MAKELPARAM(0,40));SendMessageW(hold,TBM_SETPOS,TRUE,LPARAM(std::lround(settings.holdSeconds*20)));
        control(L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,holdEdit,254,250,76,28);control(L"BUTTON",tr(L"\u9ed8\u8ba4",L"Default"),WS_TABSTOP|BS_PUSHBUTTON,holdReset,348,250,68,28);setNumeric(holdEdit,settings.holdSeconds);
        control(L"BUTTON",tr(L"全屏应用中自动停用",L"Disable in fullscreen apps"),WS_TABSTOP|BS_AUTOCHECKBOX,fullscreenCheck,24,300,392,24);CheckDlgButton(settingsWindow,fullscreenCheck,settings.fullscreen?BST_CHECKED:BST_UNCHECKED);
        control(L"BUTTON",tr(L"登录 Windows 时自动启动",L"Start when I sign in to Windows"),WS_TABSTOP|BS_AUTOCHECKBOX,startupCheck,24,332,392,24);CheckDlgButton(settingsWindow,startupCheck,startupEnabled()?BST_CHECKED:BST_UNCHECKED);
        control(L"STATIC",tr(L"排除应用（进程名，逗号分隔，如 game.exe）",L"Excluded apps (comma-separated, e.g. game.exe)"),0,0,24,369,392,24);control(L"EDIT",settings.excluded.c_str(),WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|ES_WANTRETURN,excludedEdit,24,398,392,66);
        control(L"BUTTON",tr(L"关闭",L"Close"),WS_TABSTOP|BS_DEFPUSHBUTTON,IDOK,24,485,392,32);
        initializingSettings=false;ShowWindow(settingsWindow,SW_SHOW);SetForegroundWindow(settingsWindow);SetFocus(GetDlgItem(settingsWindow,scaleEdit));
    }
};
static App* getApp(HWND window){return reinterpret_cast<App*>(GetWindowLongPtrW(window,GWLP_USERDATA));}
static LRESULT CALLBACK mainProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));}
    App* app=getApp(window);if(!app)return DefWindowProcW(window,message,wp,lp);
    if(app->taskbarMessage&&message==app->taskbarMessage){app->addTray();return 0;}
    try{
        if(message==trayMessage){if(lp==WM_LBUTTONDBLCLK)app->openSettings();else if(lp==WM_RBUTTONUP||lp==WM_CONTEXTMENU)app->menu();return 0;}
        if(message==WM_COMMAND){switch(LOWORD(wp)){case idEnable:app->enabled=!app->enabled;app->stop();break;case idSettings:app->openSettings();break;case idQuit:PostMessageW(window,WM_CLOSE,0,0);break;}return 0;}
        if(message==WM_CLOSE){app->stop();app->quitting=true;PostQuitMessage(0);return 0;}
        if(message==WM_DISPLAYCHANGE||message==WM_SETTINGCHANGE){app->pointerMonitor=nullptr;return 0;}
        if(message==WM_QUERYENDSESSION){app->stop();return TRUE;}
    }catch(const std::exception& ex){auto s=errorText(ex);MessageBoxW(window,s.c_str(),L"WhereIsMyCursor",MB_OK|MB_ICONERROR);}
    return DefWindowProcW(window,message,wp,lp);
}
static LRESULT CALLBACK settingsProc(HWND window,UINT message,WPARAM wp,LPARAM lp){
    if(message==WM_NCCREATE){auto create=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));}
    App* app=getApp(window);if(!app)return DefWindowProcW(window,message,wp,lp);
    switch(message){
        case WM_HSCROLL:{int id=GetDlgCtrlID(reinterpret_cast<HWND>(lp));if(id==scaleSlider||id==sensSlider||id==holdSlider){auto value=SendMessageW(reinterpret_cast<HWND>(lp),TBM_GETPOS,0,0);int edit=id==scaleSlider?scaleEdit:(id==holdSlider?holdEdit:sensEdit);app->setNumeric(edit,value*(id==scaleSlider?.5:.05));app->numericChanged(edit);}return 0;}
        case WM_COMMAND:{
            int id=LOWORD(wp),notification=HIWORD(wp);
            if(app->initializingSettings)return 0;
            if(id==IDOK||id==IDCANCEL){app->closeSettings();return 0;}
            if(id==scaleReset||id==sensReset||id==holdReset){int edit=id==scaleReset?scaleEdit:(id==holdReset?holdEdit:sensEdit);app->setNumeric(edit,id==scaleReset?8:(id==holdReset?Settings::DefaultHoldSeconds:Settings::DefaultSensitivity));app->editCommit(edit);return 0;}
            if(id==scaleEdit||id==sensEdit||id==holdEdit){if(notification==EN_CHANGE)app->numericChanged(id);else if(notification==EN_KILLFOCUS)app->editCommit(id);return 0;}
            if(id==excludedEdit&&notification==EN_CHANGE){app->settings.excluded=text(GetDlgItem(window,id));app->changed();return 0;}
            if(id==fullscreenCheck&&notification==BN_CLICKED){app->settings.fullscreen=IsDlgButtonChecked(window,id)==BST_CHECKED;app->changed();return 0;}
            if(id==startupCheck&&notification==BN_CLICKED){
                try{setStartup(IsDlgButtonChecked(window,id)==BST_CHECKED);}
                catch(const std::exception& ex){CheckDlgButton(window,id,startupEnabled()?BST_CHECKED:BST_UNCHECKED);auto message=errorText(ex);MessageBoxW(window,message.c_str(),L"WhereIsMyCursor",MB_OK|MB_ICONERROR);}return 0;
            }return 0;
        }
        case WM_TIMER:if(wp==1)app->persist();return 0;
        case WM_DPICHANGED:{app->dpi=HIWORD(wp);auto rect=reinterpret_cast<RECT*>(lp);SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);app->resizeControls();return 0;}
        case WM_CTLCOLORSTATIC:if(GetDlgCtrlID(reinterpret_cast<HWND>(lp))>=scaleHelp&&GetDlgCtrlID(reinterpret_cast<HWND>(lp))<=holdHelp)SetTextColor(reinterpret_cast<HDC>(wp),GetSysColor(COLOR_GRAYTEXT));SetBkMode(reinterpret_cast<HDC>(wp),TRANSPARENT);return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        case WM_CLOSE:app->closeSettings();return 0;
        case WM_DESTROY:app->settingsWindow=nullptr;app->controls.clear();return 0;
    }
    return DefWindowProcW(window,message,wp,lp);
}
void uiTest(HINSTANCE instance){
    require(chineseUI(0x0804)&&chineseUI(0x0404)&&chineseUI(0x0c04)&&!chineseUI(0x0409)&&!chineseUI(0x0411)&&!chineseUI(0x0407),"System UI language fallback failed");
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
    for(LANGID language:{LANGID(0x0804),LANGID(0x0409)}){
    App app(instance,language);app.settingsOverride=exePath()+L".ui-test.json";app.openSettings();
    require(GetDlgItem(app.settingsWindow,scaleSlider)&&GetDlgItem(app.settingsWindow,sensSlider),"Native sliders missing");
    for(int id:{scaleSlider,sensSlider,holdSlider}){
        HWND slider=GetDlgItem(app.settingsWindow,id);SendMessageW(slider,TBM_SETPOS,TRUE,20);
        SendMessageW(slider,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
        require(SendMessageW(slider,TBM_GETPOS,0,0)>20,"Wheel up did not increase slider value");
        SendMessageW(slider,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);
        require(SendMessageW(slider,TBM_GETPOS,0,0)==20,"Wheel down did not decrease slider value");
        SendMessageW(slider,TBM_SETPOS,TRUE,SendMessageW(slider,TBM_GETRANGEMAX,0,0));
        SendMessageW(slider,WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),0);
        require(SendMessageW(slider,TBM_GETPOS,0,0)==SendMessageW(slider,TBM_GETRANGEMAX,0,0),"Wheel exceeded slider maximum");
        SendMessageW(slider,TBM_SETPOS,TRUE,SendMessageW(slider,TBM_GETRANGEMIN,0,0));
        SendMessageW(slider,WM_MOUSEWHEEL,MAKEWPARAM(0,-WHEEL_DELTA),0);
        require(SendMessageW(slider,TBM_GETPOS,0,0)==SendMessageW(slider,TBM_GETRANGEMIN,0,0),"Wheel exceeded slider minimum");
    }
    SetDlgItemTextW(app.settingsWindow,scaleEdit,L"99");double value=0;require(!app.numeric(scaleEdit,2,20,.5,value,false),"Out-of-range scale accepted");
    SetDlgItemTextW(app.settingsWindow,sensEdit,L"NaN");require(!app.numeric(sensEdit,.25,2,.05,value,false),"NaN sensitivity accepted");
    SetDlgItemTextW(app.settingsWindow,scaleEdit,L"10.5");app.editCommit(scaleEdit);require(SendDlgItemMessageW(app.settingsWindow,scaleSlider,TBM_GETPOS,0,0)==21,"Numeric edit did not update slider");
    SendMessageW(app.settingsWindow,WM_COMMAND,scaleReset,0);require(text(GetDlgItem(app.settingsWindow,scaleEdit))==L"8","Scale reset failed");
    SendMessageW(app.settingsWindow,WM_COMMAND,sensReset,0);require(text(GetDlgItem(app.settingsWindow,sensEdit))==L"0.8","Sensitivity reset failed");
    require(app.settings.scale==8&&app.settings.sensitivity==Settings::DefaultSensitivity,"Reset did not apply immediately");
    SetDlgItemTextW(app.settingsWindow,scaleEdit,L"12");require(app.settings.scale==12,"Typing did not apply immediately");
    SetDlgItemTextW(app.settingsWindow,scaleEdit,L"");require(app.settings.scale==12,"Incomplete input changed active settings");app.editCommit(scaleEdit);
    SendDlgItemMessageW(app.settingsWindow,sensSlider,TBM_SETPOS,TRUE,16);SendMessageW(app.settingsWindow,WM_HSCROLL,TB_THUMBTRACK,reinterpret_cast<LPARAM>(GetDlgItem(app.settingsWindow,sensSlider)));require(app.settings.sensitivity==.8,"Slider did not apply immediately");
    SetDlgItemTextW(app.settingsWindow,excludedEdit,L"example.exe");SendMessageW(app.settingsWindow,WM_COMMAND,MAKEWPARAM(excludedEdit,EN_CHANGE),reinterpret_cast<LPARAM>(GetDlgItem(app.settingsWindow,excludedEdit)));require(app.settings.excluded==L"example.exe","Exclusions did not apply immediately");
    CheckDlgButton(app.settingsWindow,fullscreenCheck,BST_UNCHECKED);SendMessageW(app.settingsWindow,WM_COMMAND,MAKEWPARAM(fullscreenCheck,BN_CLICKED),0);require(!app.settings.fullscreen,"Checkbox did not apply immediately");
    SendMessageW(app.settingsWindow,WM_TIMER,1,0);Settings persisted;persisted.load(app.settingsOverride);require(persisted.scale==12&&persisted.sensitivity==.8&&!persisted.fullscreen&&persisted.excluded==L"example.exe","Automatic persistence failed");
    require(text(GetDlgItem(app.settingsWindow,IDOK))==app.tr(L"\u5173\u95ed",L"Close"),"Close button label incorrect");
    SetDlgItemTextW(app.settingsWindow,holdEdit,L"0.7");require(std::abs(app.settings.holdSeconds-.7)<1e-9,"Hold edit did not apply immediately");
    SendDlgItemMessageW(app.settingsWindow,holdSlider,TBM_SETPOS,TRUE,20);SendMessageW(app.settingsWindow,WM_HSCROLL,TB_THUMBTRACK,reinterpret_cast<LPARAM>(GetDlgItem(app.settingsWindow,holdSlider)));require(app.settings.holdSeconds==1,"Hold slider did not apply immediately");
    SendMessageW(app.settingsWindow,WM_COMMAND,holdReset,0);require(std::abs(app.settings.holdSeconds-Settings::DefaultHoldSeconds)<1e-9,"Hold default failed");
    app.persist();persisted.load(app.settingsOverride);require(persisted.holdSeconds==Settings::DefaultHoldSeconds,"Hold setting did not persist");
    RedrawWindow(app.settingsWindow,nullptr,nullptr,RDW_UPDATENOW|RDW_ALLCHILDREN|RDW_INVALIDATE);
    UpdateWindow(app.settingsWindow);
    RECT previewRect{};GetWindowRect(app.settingsWindow,&previewRect);Dib preview;preview.ensure(previewRect.right-previewRect.left,previewRect.bottom-previewRect.top);
    require(PrintWindow(app.settingsWindow,preview.dc,2),"Settings preview render failed");GdiFlush();
    BITMAPFILEHEADER fileHeader{};fileHeader.bfType=0x4D42;fileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);fileHeader.bfSize=fileHeader.bfOffBits+preview.width*preview.height*4;
    BITMAPINFOHEADER imageHeader{};imageHeader.biSize=sizeof(imageHeader);imageHeader.biWidth=preview.width;imageHeader.biHeight=-preview.height;imageHeader.biPlanes=1;imageHeader.biBitCount=32;
    auto previewPath=exePath();previewPath.resize(previewPath.find_last_of(L'\\')+1);previewPath+=app.chinese?L"native-settings-preview-zh.bmp":L"native-settings-preview-en.bmp";
    HANDLE image=CreateFileW(previewPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);require(image!=INVALID_HANDLE_VALUE,"Cannot write settings preview");DWORD bytes=0;
    WriteFile(image,&fileHeader,sizeof(fileHeader),&bytes,nullptr);WriteFile(image,&imageHeader,sizeof(imageHeader),&bytes,nullptr);WriteFile(image,preview.pixels,preview.width*preview.height*4,&bytes,nullptr);CloseHandle(image);
    app.dpi=144;app.resizeControls();RECT r{};GetWindowRect(GetDlgItem(app.settingsWindow,scaleEdit),&r);require(r.right-r.left==114,"DPI layout failed");
    SetDlgItemTextW(app.settingsWindow,scaleEdit,L"14");SendMessageW(app.settingsWindow,WM_COMMAND,IDOK,0);require(!app.settingsWindow,"Close button did not close settings");persisted.load(app.settingsOverride);require(persisted.scale==14,"Closing lost pending settings");DeleteFileW(app.settingsOverride.c_str());
    }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    int count=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&count);std::vector<std::wstring> args;for(int i=1;i<count;i++)args.emplace_back(argv[i]);LocalFree(argv);
    try{
        if(args.size()==1&&args[0]==L"--render-benchmark")return renderBenchmark();
        if(args.size()==1&&args[0]==L"--recovery-test"){CursorVisibility visibility;visibility.hide();Sleep(2500);ExitProcess(77);}
        if(args.size()==1&&(args[0]==L"--self-test"||args[0]==L"--integration-test"))return selfTest(args[0]==L"--integration-test");
        if(args.size()==1&&args[0]==L"--quit"){HWND window=FindWindowW(L"WhereIsMyCursor.NativeMain",nullptr);if(window)PostMessageW(window,WM_CLOSE,0,0);return 0;}
        HANDLE mutex=CreateMutexW(nullptr,TRUE,L"Local\\ShakeCursor");if(!mutex||GetLastError()==ERROR_ALREADY_EXISTS){if(mutex)CloseHandle(mutex);return 0;}
        INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
        try{if(startupEnabled())setStartup(true);}catch(...){}
        {
            App app(instance);MSG message{};u64 next=GetTickCount64();
            while(!app.quitting){
                while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
                    if(message.message==WM_QUIT){app.quitting=true;break;}
                    if(app.settingsWindow&&IsDialogMessageW(app.settingsWindow,&message))continue;
                    TranslateMessage(&message);DispatchMessageW(&message);
                }
                if(app.quitting)break;u64 now=GetTickCount64();if(now>=next){app.tick(now);next=now+(app.animation.active(now)?4:16);}
                now=GetTickCount64();DWORD timeout=next>now?DWORD(next-now):0;MsgWaitForMultipleObjectsEx(0,nullptr,timeout,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
            }
        }
        ReleaseMutex(mutex);CloseHandle(mutex);return 0;
    }catch(const std::exception& ex){cursorApiShow(TRUE);auto message=std::wstring(uiText(chineseUI(GetUserDefaultUILanguage()),L"\u7a0b\u5e8f\u65e0\u6cd5\u7ee7\u7eed\u8fd0\u884c\uff1a",L"The application cannot continue: "))+errorText(ex);MessageBoxW(nullptr,message.c_str(),L"WhereIsMyCursor",MB_OK|MB_ICONERROR);return 1;}
}
