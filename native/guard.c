#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif
#include <windows.h>

// Intentionally C and kernel32-only while waiting: do not load GUI or
// magnification libraries until recovery is actually needed.
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command,int show){
    (void)instance;(void)previous;(void)show;
    while(*command==L' ')command++;
    DWORD pid=0;WCHAR* digits=command;
    while(*command>=L'0'&&*command<=L'9'){pid=pid*10+(*command-L'0');command++;}
    if(!pid)return 1;
    WCHAR name[80]=L"Local\\WhereIsMyCursor.Guard.";int n=0;while(name[n])n++;
    for(WCHAR* p=digits;p<command&&n<78;p++)name[n++]=*p;name[n]=0;
    HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid),ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,name);
    if(!parent||!ready){if(parent)CloseHandle(parent);if(ready)CloseHandle(ready);return 2;}
    SetEvent(ready);CloseHandle(ready);WaitForSingleObject(parent,INFINITE);CloseHandle(parent);
    HMODULE library=LoadLibraryW(L"Magnification.dll");if(!library)return 3;
    typedef BOOL (WINAPI* Initialize)(void);typedef BOOL (WINAPI* Show)(BOOL);
    Initialize init=(Initialize)GetProcAddress(library,"MagInitialize"),uninit=(Initialize)GetProcAddress(library,"MagUninitialize");
    Show visible=(Show)GetProcAddress(library,"MagShowSystemCursor");
    BOOL restored=FALSE;if(init&&uninit&&visible&&init()){restored=visible(TRUE);uninit();}FreeLibrary(library);
    return restored?0:4;
}
