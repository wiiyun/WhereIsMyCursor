#include "core.h"
#include <cstdlib>

std::wstring exePath(){std::vector<wchar_t> value(32768);DWORD n=GetModuleFileNameW(nullptr,value.data(),DWORD(value.size()));require(n>0&&n<value.size(),"Cannot resolve executable");return {value.data(),n};}
static std::wstring utf16(const std::string& s){if(s.empty())return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);require(n>0,"Invalid UTF-8 settings");std::wstring out(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),out.data(),n);return out;}
static std::string utf8(const std::wstring& s){if(s.empty())return {};int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string out(n,0);WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),out.data(),n,nullptr,nullptr);return out;}
std::wstring settingsPath(){wchar_t root[MAX_PATH]{};require(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_LOCAL_APPDATA,nullptr,SHGFP_TYPE_CURRENT,root)),"LocalAppData unavailable");std::wstring folder=std::wstring(root)+L"\\ShakeCursor";CreateDirectoryW(folder.c_str(),nullptr);return folder+L"\\settings.json";}
static size_t field(const std::wstring& json,const wchar_t* key){auto pos=json.find(L"\""+std::wstring(key)+L"\"");if(pos==std::wstring::npos)return pos;pos=json.find(L':',pos);if(pos==std::wstring::npos)return pos;do{pos++;}while(pos<json.size()&&iswspace(json[pos]));return pos;}
static double number(const std::wstring& json,const wchar_t* key,double fallback){size_t p=field(json,key);if(p==std::wstring::npos)return fallback;wchar_t* end=nullptr;double v=wcstod(json.c_str()+p,&end);return end!=json.c_str()+p&&std::isfinite(v)?v:fallback;}
static std::wstring stringField(const std::wstring& json,const wchar_t* key){size_t p=field(json,key);if(p==std::wstring::npos||p>=json.size()||json[p++]!=L'"')return {};std::wstring out;while(p<json.size()){wchar_t c=json[p++];if(c==L'"')return out;if(c!=L'\\'){out+=c;continue;}if(p>=json.size())break;c=json[p++];if(c==L'n')out+=L'\n';else if(c==L'r')out+=L'\r';else if(c==L't')out+=L'\t';else if(c==L'u'&&p+4<=json.size()){wchar_t hex[5]{};for(int i=0;i<4;i++)hex[i]=json[p++];out+=wchar_t(wcstoul(hex,nullptr,16));}else out+=c;}throw std::runtime_error("Invalid JSON string");}
static std::wstring escape(const std::wstring& s){std::wstring out;for(wchar_t c:s){if(c==L'"'||c==L'\\'){out+=L'\\';out+=c;}else if(c==L'\n')out+=L"\\n";else if(c==L'\r')out+=L"\\r";else if(c==L'\t')out+=L"\\t";else if(c>=32)out+=c;}return out;}
void Settings::load(const std::wstring& path){
    HANDLE file=CreateFileW((path.empty()?settingsPath():path).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);if(file==INVALID_HANDLE_VALUE)return;
    DWORD size=GetFileSize(file,nullptr);if(size>1024*1024){CloseHandle(file);return;}std::string bytes(size,0);DWORD read=0;bool ok=ReadFile(file,bytes.data(),size,&read,nullptr);CloseHandle(file);if(!ok)return;
    try{auto json=utf16(bytes);holdSeconds=std::clamp(number(json,L"HoldSeconds",DefaultHoldSeconds),0.,2.);scale=std::clamp(number(json,L"Scale",8),2.,20.);sensitivity=std::clamp(number(json,L"Sensitivity",DefaultSensitivity),.25,2.);if(number(json,L"Version",0)<3)sensitivity=DefaultSensitivity;if(number(json,L"Version",0)<4)sensitivity=std::min(sensitivity,DefaultSensitivity);size_t p=field(json,L"SkipFullscreen");fullscreen=p==std::wstring::npos||json.compare(p,5,L"false")!=0;excluded=stringField(json,L"ExcludedProcesses");}catch(...){*this=Settings{};}
}
void Settings::save(const std::wstring& overridePath)const{
    wchar_t numeric[200];swprintf(numeric,200,L"{\"Version\":5,\"HoldSeconds\":%.2f,\"Scale\":%.2f,\"Sensitivity\":%.2f,\"SkipFullscreen\":%ls,\"ExcludedProcesses\":\"",holdSeconds,scale,sensitivity,fullscreen?L"true":L"false");
    auto bytes=utf8(std::wstring(numeric)+escape(excluded)+L"\"}");std::wstring path=overridePath.empty()?settingsPath():overridePath,temp=path+L".tmp";
    HANDLE file=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);require(file!=INVALID_HANDLE_VALUE,"Cannot save settings");DWORD count=0;bool ok=WriteFile(file,bytes.data(),DWORD(bytes.size()),&count,nullptr);FlushFileBuffers(file);CloseHandle(file);require(ok&&count==bytes.size(),"Cannot write settings");require(MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH),"Cannot replace settings");
}
static constexpr const wchar_t* runKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
bool startupEnabled(){HKEY key;if(RegOpenKeyExW(HKEY_CURRENT_USER,runKey,0,KEY_QUERY_VALUE,&key)!=ERROR_SUCCESS)return false;bool enabled=false;for(auto name:{L"WhereIsMyCursor",L"WhereIsMyMouse",L"ShakeCursor",L"我的鼠标在哪里"}){DWORD type=0,size=0;if(RegQueryValueExW(key,name,nullptr,&type,nullptr,&size)==ERROR_SUCCESS&&type==REG_SZ&&size>sizeof(wchar_t))enabled=true;}RegCloseKey(key);return enabled;}
void setStartup(bool enabled){HKEY key;require(RegCreateKeyExW(HKEY_CURRENT_USER,runKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)==ERROR_SUCCESS,"Cannot open startup configuration");std::wstring command=L"\""+exePath()+L"\"";LONG result=enabled?RegSetValueExW(key,L"WhereIsMyCursor",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t))):RegDeleteValueW(key,L"WhereIsMyCursor");if(result==ERROR_SUCCESS||(!enabled&&result==ERROR_FILE_NOT_FOUND)){for(auto old:{L"WhereIsMyMouse",L"ShakeCursor",L"我的鼠标在哪里"})RegDeleteValueW(key,old);}RegCloseKey(key);require(result==ERROR_SUCCESS||(!enabled&&result==ERROR_FILE_NOT_FOUND),"Cannot update startup configuration");}
bool ShakeDetector::add(u64 now,double x,double y,double sensitivity){
    while(!points.empty()&&now-points.front().time>500)points.pop_front();if(previous&&x==lastX&&y==lastY)return false;lastX=x;lastY=y;previous=true;points.push_back({now,x,y});if(points.size()<8)return false;
    sensitivity=std::clamp(sensitivity,.25,2.);double minX=x,maxX=x,minY=y,maxY=y,distance=0;auto prev=points.front();
    for(auto p:points){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);distance+=std::hypot(p.x-prev.x,p.y-prev.y);prev=p;}
    bool horizontal=maxX-minX>=maxY-minY;if(std::max(maxX-minX,maxY-minY)<80/sensitivity||distance<650/sensitivity)return false;u64 elapsed=now-points.front().time;if(elapsed<120||distance/elapsed*1000<1100/sensitivity)return false;
    double origin=horizontal?points.front().x:points.front().y,extreme=origin,excursion=45/sensitivity;int direction=0,turns=0;
    for(auto p:points){double value=horizontal?p.x:p.y;if(direction==0){if(std::abs(value-origin)>=excursion){direction=value>origin?1:-1;extreme=value;}continue;}
        if(direction>0){if(value>extreme)extreme=value;else if(extreme-value>=excursion){turns++;direction=-1;extreme=value;}}
        else {if(value<extreme)extreme=value;else if(value-extreme>=excursion){turns++;direction=1;extreme=value;}}}
    return turns>=5;
}
static std::wstring lower(std::wstring s){for(auto& c:s)c=towlower(c);return s;}
static std::wstring stem(std::wstring s){size_t slash=s.find_last_of(L"\\/");if(slash!=std::wstring::npos)s.erase(0,slash+1);auto dot=s.find_last_of(L'.');if(dot!=std::wstring::npos)s.resize(dot);return lower(s);}
bool shouldBlock(const Settings& settings){
    HWND window=GetForegroundWindow();if(!window)return true;
    if(!settings.excluded.empty()){
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return true;
        wchar_t path[32768];DWORD size=32768;bool ok=QueryFullProcessImageNameW(process,0,path,&size);CloseHandle(process);if(!ok)return true;std::wstring name=stem({path,size}),token;
        auto check=[&](){size_t a=token.find_first_not_of(L" \t"),b=token.find_last_not_of(L" \t");bool match=a!=std::wstring::npos&&stem(token.substr(a,b-a+1))==name;token.clear();return match;};
        for(wchar_t c:settings.excluded){if(c==L','||c==L';'||c==L'\r'||c==L'\n'){if(check())return true;}else token+=c;}if(check())return true;
    }
    if(!settings.fullscreen)return false;
    wchar_t cls[256]{};GetClassNameW(window,cls,256);if(wcscmp(cls,L"Progman")==0||wcscmp(cls,L"WorkerW")==0||wcscmp(cls,L"Shell_TrayWnd")==0)return false;
    RECT r{};MONITORINFO monitor{sizeof(monitor)};if(!GetWindowRect(window,&r)||!GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return true;auto s=monitor.rcMonitor;LONG_PTR style=GetWindowLongPtrW(window,GWL_STYLE);
    bool covers=r.left<=s.left+2&&r.top<=s.top+2&&r.right>=s.right-2&&r.bottom>=s.bottom-2;
    return covers&&((style&WS_CAPTION)!=WS_CAPTION||(r.left==s.left&&r.top==s.top&&r.right==s.right&&r.bottom==s.bottom));
}
