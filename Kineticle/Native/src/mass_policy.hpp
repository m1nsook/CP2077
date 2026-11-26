#pragma once
#include <cstdint>
#include <cmath>
namespace kineticle {
// Never use a positive native sample to override a negative/stale controller.
template<class Scope> const Scope& chooseScope(bool controllerClaimed,const Scope& controller,const Scope& native){return controllerClaimed?controller:native;}
inline bool validMultiplier(int m){return m>=1&&m<=100;}
inline bool massScope(std::uint64_t id,std::uint64_t selected,bool candidate,unsigned physicsState,
    bool allowed,std::int64_t age,int multiplier){
    return id&&id==selected&&candidate&&physicsState==0&&allowed&&age>=0&&age<=250000&&
        validMultiplier(multiplier)&&multiplier>1;
}
inline float desiredChassisMass(float base,int multiplier){
    if(!std::isfinite(base)||base<100.f||base>15000.f||!validMultiplier(multiplier))return 0;
    const float result=base*multiplier;
    return result<=100000.f?result:100000.f;
}
}
