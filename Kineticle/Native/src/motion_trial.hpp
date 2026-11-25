#pragma once
#include "contact_trace.hpp"
#include "diagnostic_policy.hpp"
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace kineticle {
enum class TrialReject:unsigned {
    None, Guards, Context, Event, Overlap, Expired, Sample, Timing, Momentum, Direction,
    NewCorrection, Drift, Speed, Disabled, Count
};
inline const char* trialReason(TrialReject r) {
    constexpr const char* names[]={"none","guards","context","event","overlap","expired","sample","timing",
        "momentum","direction","new-correction","drift","speed","disabled"};
    static_assert(sizeof(names)/sizeof(*names)==static_cast<unsigned>(TrialReject::Count));
    return names[static_cast<unsigned>(r)];
}
inline Vec horizontal(Vec v) {v.z=0;return v;}
inline Vec limited(Vec v,float cap) {auto l=length(v);return l>cap?scale(v,cap/l):v;}
inline bool trialSample(const TraceSample& s,bool driver) {
    return s.id&&s.massOk&&validMass(s.mass,s.inverseMass)&&
        std::abs(s.mass*s.inverseMass-1.0f)<0.10f&&s.physicsState==0&&s.candidate==driver&&
        std::isfinite(s.dt)&&s.dt>0&&s.dt<=0.1f&&valid(s.before)&&valid(s.after)&&
        std::isfinite(s.change)&&s.change>=0&&std::isfinite(s.loss);
}
struct MotionPlan {
    TrialReject reason{TrialReject::None};
    Vec driverAdd{},targetAdd{};
    float error{};
};
inline MotionPlan planMotion(const TraceSample& d,const TraceSample& t) {
    MotionPlan p;
    if(!trialSample(d,true)||!trialSample(t,false)||d.id==t.id) {p.reason=TrialReject::Sample;return p;}
    if(std::abs(d.us-t.us)>5000||std::abs(d.dt-t.dt)>0.001f) {p.reason=TrialReject::Timing;return p;}
    const auto jd=scale(sub(d.after,d.before),d.mass),jt=scale(sub(t.after,t.before),t.mass);
    const auto mag=std::max(length(jd),length(jt));
    p.error=mag>0?length(add(jd,jt))/mag:1;
    if(!std::isfinite(p.error)||mag<100||p.error>0.12f) {p.reason=TrialReject::Momentum;return p;}
    const auto targetDelta=horizontal(sub(t.after,t.before));
    const auto n=length(targetDelta);
    if(n<0.25f||d.loss<0.5f) {p.reason=TrialReject::Direction;return p;}
    const auto dir=scale(targetDelta,1/n);
    // V must have been moving into the outgoing impulse, and approaching
    // the target relative to its pre-sync velocity. Not a stationary ram.
    if(dot(horizontal(d.before),dir)<0.5f||dot(horizontal(sub(d.before,t.before)),dir)<0.5f||
       dot(horizontal(sub(d.after,d.before)),dir)>=-0.25f) {p.reason=TrialReject::Direction;return p;}
    p.targetAdd=limited(targetDelta,8.0f); // extra 1x => nominal 2x, capped
    p.driverAdd=limited(horizontal(sub(recover(d.before,d.after,true,0.5f),d.after)),4.0f);
    if(!valid(p.targetAdd)||!valid(p.driverAdd)||length(p.driverAdd)<0.1f) p.reason=TrialReject::Direction;
    return p;
}
struct MotionAction {bool apply{},driver{};std::uint64_t id{};Vec delta{};};
struct MotionWrite {bool driver{};std::uint64_t id{};std::int64_t eventUs{},us{};Vec before{},after{};};
struct MotionTrial {
    static constexpr std::int64_t sampleWindowUs=45000,deadlineUs=120000;
    std::uint64_t driver{},target{},eventSequence{};
    std::int64_t eventUs{};
    bool active{},ready{},targetDone{};
    TraceSample d{},t{};
    MotionPlan plan{};
    TrialReject lastReason{TrialReject::None};
    std::uint64_t armed{},planned{},targetWrites{},driverWrites{},completed{},partial{};
    std::array<std::uint64_t,static_cast<unsigned>(TrialReject::Count)> rejections{};
    std::array<MotionWrite,16> writes{};
    std::size_t writeIndex{};
    void reject(TrialReject why) {lastReason=why;++rejections[static_cast<unsigned>(why)];}
    void cancel(TrialReject why) {if(active&&targetDone)++partial;active=false;reject(why);}
    void expire(std::int64_t now) {if(active&&(now<eventUs||now-eventUs>deadlineUs))cancel(TrialReject::Expired);}
    void arm(std::uint64_t a,std::uint64_t b,std::int64_t stamp,std::uint64_t sequence) {
        driver=a;target=b;eventUs=stamp;eventSequence=sequence;
        d={};t={};plan={};active=true;ready=false;targetDone=false;lastReason=TrialReject::None;++armed;
    }
    bool inWindow(const TraceSample& s) const {
        return s.id&&(s.id==driver||s.id==target)&&s.sequence>eventSequence&&
            s.us>=eventUs&&s.us-eventUs<=sampleWindowUs&&std::isfinite(s.change)&&s.change>=0.1f;
    }
    void offer(const TraceSample& s) {
        if(!active||ready||!inWindow(s))return;
        auto& slot=s.id==driver?d:t;
        if(!slot.id||s.us<slot.us||(s.us==slot.us&&s.sequence<slot.sequence))slot=s;
    }
    void finishPlan() {
        if(!active||ready||!d.id||!t.id)return;
        plan=planMotion(d,t);
        if(plan.reason!=TrialReject::None){cancel(plan.reason);return;}
        ready=true;++planned;
    }
    template<std::size_t N> void seed(const std::array<TraceSample,N>& samples) {
        for(const auto& s:samples)offer(s);
        finishPlan();
    }
    MotionAction step(const TraceSample& s,bool context) {
        if(!active||(s.id!=driver&&s.id!=target))return {};
        expire(s.us);if(!active)return {};
        if(!context){cancel(TrialReject::Context);return {};}
        if(!ready) {
            offer(s);finishPlan();
            if(active&&!ready&&s.us-eventUs>sampleWindowUs)cancel(TrialReject::Timing);
            return {}; // never write on the sampled collision sync
        }
        if(s.us<=std::max(d.us,t.us)||s.sequence<=std::max(d.sequence,t.sequence))return {};
        const bool isDriver=s.id==driver;
        if(!isDriver&&targetDone)return {};
        const auto& measured=isDriver?d:t;
        if(!trialSample(s,isDriver)||std::abs(s.mass-measured.mass)>measured.mass*0.01f) {
            cancel(TrialReject::Sample);return {};
        }
        // Reject any new significant correction; do not adjust a new wall,
        // ground or vehicle contact. No write on an ambiguous/new sync.
        if(s.change>0.08f){cancel(TrialReject::NewCorrection);return {};}
        if(length(sub(s.after,measured.after))>3.0f){cancel(TrialReject::Drift);return {};}
        if(isDriver&&!targetDone)return {}; // target first; no recovery without delivery
        auto delta=isDriver?plan.driverAdd:plan.targetAdd;
        const auto v=add(s.after,delta);
        if(!valid(v)||length(v)>85.0f){cancel(TrialReject::Speed);return {};}
        return {true,isDriver,s.id,delta};
    }
    // Call only after the current callback actually wrote its own body.
    void recordWrite(const MotionAction& action,Vec before,Vec after,std::int64_t now) {
        if(!active||!action.apply)return;
        writes[writeIndex++%writes.size()]={action.driver,action.id,eventUs,now,before,after};
        if(action.driver){++driverWrites;++completed;active=false;}
        else {++targetWrites;targetDone=true;}
    }
};
}
