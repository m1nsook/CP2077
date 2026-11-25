#pragma once
#include <algorithm>
#include <cmath>

namespace kineticle {
struct Vec { float x{}, y{}, z{}; };
inline Vec sub(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline float dot(Vec a, Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec scale(Vec a,float s) { return {a.x*s,a.y*s,a.z*s}; }
inline Vec add(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline bool valid(Vec a) {
    return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)
        &&std::abs(a.x)<500&&std::abs(a.y)<500&&std::abs(a.z)<500;
}
inline float length(Vec a) { return std::sqrt(dot(a,a)); }
// Pure observation. Forward loss is the component of the measured correction
// opposing the horizontal pre-sync motion, not a proof of vehicle contact.
inline float forwardLoss(Vec before,Vec after) {
    if (!valid(before)||!valid(after)) return 0;
    before.z=0;
    const auto speed=length(before);
    if (speed<0.5f) return 0;
    return std::max(0.0f,-dot(sub(after,before),scale(before,1.0f/speed)));
}
// Offline policy only, not called by the game probe.
// Recovery is allowed only for independently confirmed vehicle contacts.
// Never restore vertical speed, steering or acceleration.
inline Vec recover(Vec before,Vec after,bool confirmedVehicleContact,float retention) {
    if (!confirmedVehicleContact||!valid(before)||!valid(after)||!std::isfinite(retention)) return after;
    Vec horizontal{before.x,before.y,0};
    const auto speed=length(horizontal);
    if (speed<0.5f) return after;
    const auto direction=scale(horizontal,1.0f/speed);
    const auto loss=std::clamp(forwardLoss(before,after),0.0f,speed);
    return add(after,scale(direction,loss*std::clamp(retention,0.0f,1.0f)));
}
}
