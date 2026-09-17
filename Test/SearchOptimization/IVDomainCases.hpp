#pragma once
#include <Core/Global.hpp>
#include <array>
struct IVBounds
{
    std::array<u8, 6> min {};
    std::array<u8, 6> max {31,31,31,31,31,31};
    bool valid() const
    {
        for (size_t i=0;i<6;++i) if (min[i]>max[i] || max[i]>31) return false;
        return true;
    }
};
#include <string>
#include <vector>
struct IVDomainCase { std::string name; IVBounds bounds; };
inline std::vector<IVDomainCase> ivDomainCases()
{
    std::vector<IVDomainCase> cases;
    for(uint8_t minimum:{0,1,5,10,15,20,24,25,26,27,28,29,30,31}) {
        IVBounds bounds;bounds.min.fill(minimum);
        cases.push_back({std::to_string(minimum)+"-31",bounds});
    }
    cases.push_back({"user-mixed",{{0,31,10,25,5,30},{31,31,20,31,15,31}}});
    cases.push_back({"alternating",{{0,10,0,15,0,20},{5,20,10,25,15,31}}});
    cases.push_back({"low-maxima",{{0,0,0,0,0,0},{10,10,10,10,10,10}}});
    cases.push_back({"middle",{{10,10,10,10,10,10},{20,20,20,20,20,20}}});
    cases.push_back({"low-speed-cache",{{30,31,30,0,30,0},{31,31,31,31,31,1}}});
    cases.push_back({"narrow-mixed",{{0,10,20,25,5,30},{3,15,25,31,10,31}}});
    return cases;
}
