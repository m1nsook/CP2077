#pragma once
#include "physics.hpp"
#include <cstdint>

namespace kineticle {
inline bool oneDriver(int a,int b) {
    return (a==1&&b==0)||(a==0&&b==1);
}
inline bool nativeGuards(int driver,int target,int remote,int quest,int destroyed,int targetDestroyed,int targetQuest,
    bool road,bool candidate,bool targetCandidate) {
    return driver==1&&target==0&&remote==0&&quest==0&&destroyed==0&&targetDestroyed==0&&
        targetQuest==0&&road&&candidate&&!targetCandidate;
}
// A near-duplicate is only a label; never a proof that contacts are identical.
inline bool nearPair(std::uint64_t a,std::uint64_t b,std::int64_t t,
    std::uint64_t c,std::uint64_t d,std::int64_t old) {
    return a&&b&&c&&d&&t>=old&&t-old<=50000&&((a==c&&b==d)||(a==d&&b==c));
}
inline bool validMass(float mass,float inverse) {
    return std::isfinite(mass)&&std::isfinite(inverse)&&mass>=25&&mass<=50000&&inverse>0&&inverse<=1;
}
struct Estimate {float impulse{},extra5{},extra100{};Vec recover50{},recover100{};};
inline Estimate estimate(Vec before,Vec after,float mass,bool massOk) {
    Estimate e;
    if(!valid(before)||!valid(after))return e;
    e.recover50=recover(before,after,true,.5f);
    e.recover100=recover(before,after,true,1);
    if(massOk&&std::isfinite(mass)&&mass>0) {
        e.impulse=mass*length(sub(after,before));
        e.extra5=e.impulse*4;e.extra100=e.impulse*99;
    }
    return e;
}
}
