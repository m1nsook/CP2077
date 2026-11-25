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
// Traffic state 1 is supported ONLY for measuring the struck vehicle's
// collision correction. A body must be dynamic (state 0) for ANY write.
inline bool trialSample(const TraceSample& s,bool driver,bool write=false) {
    const bool stateOk=s.physicsState==0||(!write&&!driver&&s.physicsState==1);
    return s.id&&s.massOk&&validMass(s.mass,s.inverseMass)&&
        std::abs(s.mass*s.inverseMass-1.0f)<0.10f&&stateOk&&s.candidate==driver&&
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
    // Preserve full 3-D conservation check: do not broaden it to a
    // horizontal-only match that could hide an unrelated ground contact.
    if(!std::isfinite(p.error)||mag<100||p.error>0.12f) {p.reason=TrialReject::Momentum;return p;}
    const auto targetDelta=horizontal(sub(t.after,t.before));
    const auto n=length(targetDelta);
    // Accept modest but verified transfers. Never add velocity to an
    // unrelated body or to a player moving away from the outgoing impulse.
    if(n<0.15f||d.loss<0.25f) {p.reason=TrialReject::Direction;return p;}
    const auto dir=scale(targetDelta,1/n);
    if(dot(horizontal(d.before),dir)<0.5f||dot(horizontal(sub(d.before,t.before)),dir)<0.5f||
       dot(horizontal(sub(d.after,d.before)),dir)>=-0.15f) {p.reason=TrialReject::Direction;return p;}
    p.targetAdd=limited(targetDelta,8.0f);
    p.driverAdd=limited(horizontal(sub(recover(d.before,d.after,true,0.5f),d.after)),4.0f);
    if(!valid(p.targetAdd)||!valid(p.driverAdd)||length(p.driverAdd)<0.1f) p.reason=TrialReject::Direction;
    return p;
}
struct MotionAction {bool apply{},driver{};std::uint64_t id{};Vec delta{};};
struct MotionWrite {bool driver{};std::uint64_t id{};std::int64_t eventUs{},us{};Vec before{},after{};};
struct MotionAudit {
    std::uint64_t driver{},target{},eventSequence{};
    TrialReject reason{TrialReject::None};
    bool ready{},targetDone{},driverDone{};
    std::size_t driverCount{},targetCount{};
    TraceSample d{},t{};
    MotionPlan plan{};
};
struct MotionTrial {
    static constexpr std::int64_t sampleWindowUs=65000,deadlineUs=220000;
    std::uint64_t driver{},target{},eventSequence{};
    std::int64_t eventUs{};
    bool active{},ready{},targetDone{};
    TraceSample d{},t{};
    MotionPlan plan{};
    TrialReject lastReason{TrialReject::None};
    TrialReject bestFailure{TrialReject::Timing};
    std::array<TraceSample,8> driverCandidates{},targetCandidates{};
    std::size_t driverCount{},targetCount{};
    std::uint64_t armed{},planned{},targetWrites{},driverWrites{},completed{},partial{},
        trafficObservations{},trafficWaits{},contextWaits{},sameSyncWrites{};
    std::array<std::uint64_t,static_cast<unsigned>(TrialReject::Count)> rejections{};
    std::array<MotionWrite,16> writes{};
    std::array<MotionAudit,32> audit{};
    std::size_t writeIndex{},auditIndex{};
    void reject(TrialReject why) {lastReason=why;++rejections[static_cast<unsigned>(why)];}
    void saveAudit(TrialReject why,bool driverDone) {
        audit[auditIndex++%audit.size()]={driver,target,eventSequence,why,ready,targetDone,driverDone,
            driverCount,targetCount,d,t,plan};
    }
    void cancel(TrialReject why) {
        if(active){if(targetDone)++partial;saveAudit(why,false);}
        active=false;reject(why);
    }
    void expire(std::int64_t now) {
        if(active&&(now<eventUs||now-eventUs>deadlineUs))cancel(ready?TrialReject::Expired:bestFailure);
    }
    void arm(std::uint64_t a,std::uint64_t b,std::int64_t stamp,std::uint64_t sequence) {
        driver=a;target=b;eventUs=stamp;eventSequence=sequence;
        d={};t={};plan={};driverCandidates={};targetCandidates={};driverCount=targetCount=0;
        active=true;ready=false;targetDone=false;lastReason=TrialReject::None;bestFailure=TrialReject::Timing;++armed;
    }
    bool inWindow(const TraceSample& s) const {
        return s.id&&(s.id==driver||s.id==target)&&s.sequence>eventSequence&&
            s.us>=eventUs&&s.us-eventUs<=sampleWindowUs&&std::isfinite(s.change)&&s.change>=0.1f;
    }
    void offer(const TraceSample& s) {
        if(!active||ready||!inWindow(s))return;
        auto& list=s.id==driver?driverCandidates:targetCandidates;
        auto& count=s.id==driver?driverCount:targetCount;
        for(std::size_t i=0;i<count;++i)if(list[i].sequence==s.sequence)return;
        if(count<list.size()) {
            list[count++]=s;
            if(s.id==target&&s.physicsState==1)++trafficObservations;
        }
    }
    void finishPlan() {
        if(!active||ready||!driverCount||!targetCount)return;
        bool found=false;
        float bestError=2;
        std::int64_t bestTime=0;
        // Evaluate every bounded candidate pair. The first mismatch is not
        // final; the other worker may publish the matching sample next.
        for(std::size_t i=0;i<driverCount;++i)for(std::size_t j=0;j<targetCount;++j) {
            const auto& a=driverCandidates[i];const auto& b=targetCandidates[j];
            const auto candidate=planMotion(a,b);
            if(candidate.reason==TrialReject::None) {
                const auto time=std::max(a.us,b.us);
                if(!found||time<bestTime||(time==bestTime&&candidate.error<bestError)) {
                    found=true;bestTime=time;bestError=candidate.error;d=a;t=b;plan=candidate;
                }
            } else if(!found) {
                // Preserve the most informative rejected pair for the audit.
                auto rank=[](TrialReject x) {
                    switch(x){case TrialReject::Direction:return 4;case TrialReject::Momentum:return 3;
                        case TrialReject::Timing:return 2;case TrialReject::Sample:return 1;default:return 0;}
                };
                if(!d.id||rank(candidate.reason)>rank(bestFailure)||
                    (candidate.reason==bestFailure&&candidate.error<plan.error)) {
                    bestFailure=candidate.reason;d=a;t=b;plan=candidate;
                }
            }
        }
        if(found){ready=true;++planned;}
    }
    template<std::size_t N> void seed(const std::array<TraceSample,N>& samples) {
        for(const auto& s:samples)offer(s);
        finishPlan();
    }
    MotionAction step(const TraceSample& s,bool context,bool postEventContext=false) {
        if(!active||(s.id!=driver&&s.id!=target))return {};
        expire(s.us);if(!active)return {};
        if(!context){cancel(TrialReject::Context);return {};}
        if(!ready) {
            offer(s);finishPlan();
            if(!ready) {
                if(s.us-eventUs>sampleWindowUs)cancel(bestFailure);
                return {};
            }
        }
        const bool isDriver=s.id==driver;
        if(!isDriver&&targetDone)return {};
        const auto& measured=isDriver?d:t;
        // If the dynamic target callback itself completes the match, its
        // already-observed correction can be amplified now. Otherwise wait
        // for a clean later callback, never replay a previous raw body pointer.
        const bool sameTarget=!isDriver&&s.sequence==t.sequence&&s.us==t.us;
        if(!sameTarget&&(s.us<=std::max(d.us,t.us)||s.sequence<=std::max(d.sequence,t.sequence)))return {};
        if(!isDriver&&s.physicsState==1&&trialSample(s,false)) {
            ++trafficWaits;return {};
        }
        if(!trialSample(s,isDriver,true)||std::abs(s.mass-measured.mass)>measured.mass*0.01f) {
            cancel(TrialReject::Sample);return {};
        }
        if(isDriver&&!targetDone)return {};
        // Do not restore V based only on pre-impact mounted status.
        if(isDriver&&!postEventContext){++contextWaits;return {};}
        if(!sameTarget&&s.change>0.08f){cancel(TrialReject::NewCorrection);return {};}
        if(!sameTarget&&length(sub(s.after,measured.after))>3.0f){cancel(TrialReject::Drift);return {};}
        auto delta=isDriver?plan.driverAdd:plan.targetAdd;
        const auto v=add(s.after,delta);
        if(!valid(v)||length(v)>85.0f){cancel(TrialReject::Speed);return {};}
        return {true,isDriver,s.id,delta};
    }
    void recordWrite(const MotionAction& action,Vec before,Vec after,std::int64_t now) {
        if(!active||!action.apply)return;
        writes[writeIndex++%writes.size()]={action.driver,action.id,eventUs,now,before,after};
        if(action.driver){++driverWrites;++completed;saveAudit(TrialReject::None,true);active=false;}
        else {++targetWrites;targetDone=true;if(now==t.us)++sameSyncWrites;}
    }
};
}
