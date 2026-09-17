#pragma once
// Verification only: intercept this loaded DLL's WinHttpOpen import. No production
// switches, machine proxy changes, or firewall changes are used for offline tests.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <atomic>
#include <cstring>
#include <QThread>
#include <QCoreApplication>
namespace NetworkProbe
{
inline std::atomic_int calls{0};
inline bool offline=false;
inline std::atomic_bool onGui{false};
inline HINTERNET WINAPI open(LPCWSTR agent,DWORD access,LPCWSTR proxy,LPCWSTR bypass,DWORD flags)
{
    ++calls;
    if(QThread::currentThread()==QCoreApplication::instance()->thread()) onGui=true;
    if(offline) {SetLastError(ERROR_WINHTTP_CANNOT_CONNECT);return nullptr;}
    return WinHttpOpen(agent,access,proxy,bypass,flags);
}
inline bool install(HMODULE module)
{
    auto *base=reinterpret_cast<unsigned char *>(module);
    auto *dos=reinterpret_cast<IMAGE_DOS_HEADER *>(base);
    auto *nt=reinterpret_cast<IMAGE_NT_HEADERS *>(base+dos->e_lfanew);
    auto *descriptor=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    for(;descriptor->Name;++descriptor)
    {
        if(_stricmp(reinterpret_cast<char *>(base+descriptor->Name),"WINHTTP.dll")) continue;
        auto *names=reinterpret_cast<IMAGE_THUNK_DATA *>(base+descriptor->OriginalFirstThunk);
        auto *thunks=reinterpret_cast<IMAGE_THUNK_DATA *>(base+descriptor->FirstThunk);
        for(;names->u1.AddressOfData;++names,++thunks)
        {
            if(IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto *name=reinterpret_cast<IMAGE_IMPORT_BY_NAME *>(base+names->u1.AddressOfData);
            if(std::strcmp(reinterpret_cast<char *>(name->Name),"WinHttpOpen")) continue;
            DWORD old=0;
            if(!VirtualProtect(&thunks->u1.Function,sizeof(thunks->u1.Function),PAGE_READWRITE,&old)) return false;
            thunks->u1.Function=reinterpret_cast<ULONG_PTR>(&open);
            DWORD unused=0; VirtualProtect(&thunks->u1.Function,sizeof(thunks->u1.Function),old,&unused);
            return true;
        }
    }
    return false;
}
}
