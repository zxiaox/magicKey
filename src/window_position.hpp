#pragma once
#include <windows.h>
#include <algorithm>

namespace magickey {
inline void centerWindow(HWND popup, HWND owner) {
    RECT box{}, anchor{};
    if (!GetWindowRect(popup,&box)) return;
    bool hasOwner = owner && IsWindow(owner) && !IsIconic(owner) && GetWindowRect(owner,&anchor);
    MONITORINFO monitor{sizeof(monitor)};
    if (!GetMonitorInfoW(MonitorFromWindow(hasOwner ? owner : popup,MONITOR_DEFAULTTONEAREST),&monitor)) return;
    if (!hasOwner) anchor=monitor.rcWork;
    const int width=box.right-box.left, height=box.bottom-box.top;
    int x=anchor.left+(anchor.right-anchor.left-width)/2;
    int y=anchor.top+(anchor.bottom-anchor.top-height)/2;
    x=std::clamp(x,int(monitor.rcWork.left),std::max(int(monitor.rcWork.left),int(monitor.rcWork.right)-width));
    y=std::clamp(y,int(monitor.rcWork.top),std::max(int(monitor.rcWork.top),int(monitor.rcWork.bottom)-height));
    SetWindowPos(popup,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
}
struct DialogCenterContext { HWND owner; HHOOK hook; DialogCenterContext* previous; };
inline thread_local DialogCenterContext* dialogCenterContext=nullptr;
inline LRESULT CALLBACK centerDialogHook(int code, WPARAM w, LPARAM l) {
    if (code == HCBT_ACTIVATE && dialogCenterContext) {
        wchar_t type[32]{}; GetClassNameW(reinterpret_cast<HWND>(w),type,32);
        if (lstrcmpW(type,L"#32770")==0) centerWindow(reinterpret_cast<HWND>(w),dialogCenterContext->owner);
    }
    return CallNextHookEx(dialogCenterContext ? dialogCenterContext->hook : nullptr,code,w,l);
}
inline int centeredMessageBox(HWND owner, const wchar_t* text, const wchar_t* title, UINT flags) {
    DialogCenterContext context{owner,nullptr,dialogCenterContext};
    dialogCenterContext=&context;
    context.hook=SetWindowsHookExW(WH_CBT,centerDialogHook,nullptr,GetCurrentThreadId());
    int result=MessageBoxW(owner,text,title,flags);
    if (context.hook) UnhookWindowsHookEx(context.hook);
    dialogCenterContext=context.previous;
    return result;
}
}
