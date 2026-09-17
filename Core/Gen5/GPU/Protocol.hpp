#pragma once
// Windows-only experimental transport. No OpenCL imports in the search process.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <string>
#include <stdexcept>
namespace GpuWild {
constexpr uint32_t Capacity=1048576, Slots=2, ProtocolVersion=2;
struct alignas(64) Slot {
    uint32_t n,inspect,count,reserved;
    uint32_t ivMin,ivMax; // Six five-bit values, HP/Atk/Def/SpA/SpD/Spe order.
    double upload,kernel,download,wall;
    uint64_t seeds[Capacity];
    uint32_t output[Capacity*8];
};
struct Shared { uint32_t ready;char info[8192];Slot slots[Slots]; };
inline std::wstring named(const std::wstring &base,const wchar_t *kind,int slot=0){return base+kind+std::to_wstring(slot);}
inline void wincheck(bool ok,const char *what){if(!ok)throw std::runtime_error(what);}
}
