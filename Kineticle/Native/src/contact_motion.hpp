#pragma once
#include "contact_trace.hpp"
#include "diagnostic_policy.hpp"
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
namespace kineticle {
// A direct bump event names both entities. Each body's correction is used
// independently; no matching conservation equations or worker rendezvous.
// This is NOT a contact-solver callback: simultaneous contacts are ambiguous.
constexpr float outgoingFactor=5.0f, retention=.75f, recoilReduction=.5f, spinReduction=.5f;
inline Vec flat(Vec v){v.z=0;return v;}
inline Vec capVector(Vec v,float cap){const auto n=length(v);return n>cap?scale(v,cap/n):v;}
inline bool contactDirection(Vec driver,Vec target,Vec driverVelocity,Vec& out) {
    if(!valid(driverVelocity)||!std::isfinite(driver.x)||!std::isfinite(driver.y)||
       !std::isfinite(target.x)||!std::isfinite(target.y))return false;
    const auto difference=flat(sub(target,driver));const auto n=length(difference);
    if(!std::isfinite(n)||n<.15f||n>30.f)return false;
    out=scale(difference,1/n);
    return dot(flat(driverVelocity),out)>.75f;
}
struct ContactAdjustment {Vec linear{},angular{};bool valid{};};
inline ContactAdjustment contactCorrection(const TraceSample& s,Vec direction,bool player) {
    ContactAdjustment result;
    if(!s.id||!s.massOk||!validMass(s.mass,s.inverseMass)||
       !valid(s.before)||!valid(s.after)||!valid(s.angularBefore)||!valid(s.angularAfter)||
       !valid(direction)||std::abs(length(direction)-1)>0.01f||
       (s.physicsState!=0&&(player||s.physicsState!=1))||s.candidate!=player)return result;
    auto dv=flat(sub(s.after,s.before));
    auto amount=length(dv);
    // Ignore tiny corrections, vertical contacts and opposite-facing changes.
    if(amount<.12f)return result;
    if(player) {
        const auto forward=flat(s.before);
        const auto speed=length(forward);
        if(speed<.75f||dot(forward,direction)<.5f||dot(dv,direction)>-.12f||
           -dot(dv,direction)<amount*.25f)return result;
        const auto axis=scale(forward,1/speed);
        const auto loss=std::clamp(-dot(dv,axis),0.f,speed);
        if(loss<.12f)return result;
        const auto forwardCorrection=scale(axis,-loss);
        // Recover forward momentum and reduce only the measured lateral kick.
        const auto side=sub(dv,scale(axis,dot(dv,axis)));
        result.linear=capVector(add(scale(forwardCorrection,-retention),scale(side,-recoilReduction)),8.f);
        result.angular=capVector(scale(sub(s.angularAfter,s.angularBefore),-spinReduction),3.f);
    } else {
        const float along=dot(dv,direction);
        if(along<.12f||along<amount*.30f)return result;
        // Use the measured outgoing horizontal correction, not a mass
        // estimate or the unreliable event impactVelocityChange scalar.
        result.linear=capVector(scale(dv,outgoingFactor-1.f),12.f);
    }
    result.valid=valid(result.linear)&&valid(result.angular)&&length(result.linear)>.05f;
    return result;
}
struct ContactSide {
    std::uint64_t id{},sampleSequence{};
    bool measured{},delivered{};
    TraceSample sample{};
    ContactAdjustment adjustment{};
};
struct ContactAudit {
    std::uint64_t driver{},target{},eventSequence{};
    std::int64_t eventUs{};
    ContactSide v{},npc{};
    const char* reason{"none"};
};
struct ContactMotion {
    static constexpr std::int64_t observeUs=90000,ttlUs=220000,cooldownUs=160000;
    bool active{};
    std::uint64_t driver{},target{},eventSequence{};
    std::int64_t eventUs{},lastArmUs{};
    Vec direction{};
    ContactSide v{},npc{};
    std::uint64_t armed{},duplicates{},overlaps{},expired{},contextCancelled{},driverLoss{},guardFailed{},
       directionFailed{},vMeasured{},npcMeasured{},trafficMeasured{},vWrites{},npcWrites{},writeRejected{},integratorCalls{};
    std::array<ContactAudit,24> audit{};std::size_t auditIndex{};
    void close(const char* why) {
        if(active)audit[auditIndex++%audit.size()]={driver,target,eventSequence,eventUs,v,npc,why};
        active=false;
    }
    void expire(std::int64_t stamp) {if(active&&stamp>=eventUs&&stamp-eventUs>ttlUs){++expired;close("expired");}}
    bool arm(std::uint64_t a,std::uint64_t b,std::uint64_t seq,std::int64_t stamp,Vec dir) {
        expire(stamp);
        if(active&&a==driver&&b==target&&stamp>=lastArmUs&&stamp-lastArmUs<=cooldownUs){++duplicates;return false;}
        if(!active&&a==driver&&b==target&&stamp>=lastArmUs&&stamp-lastArmUs<=cooldownUs){++duplicates;return false;}
        if(active){++overlaps;close("new-pair");}
        if(!a||!b||a==b||!valid(dir))return false;
        driver=a;target=b;eventSequence=seq;eventUs=lastArmUs=stamp;direction=dir;v={};npc={};v.id=a;npc.id=b;
        active=true;++armed;return true;
    }
    bool includes(std::uint64_t id) const {return active&&(id==driver||id==target);}
    void offer(const TraceSample& s) {
        if(!includes(s.id)||s.sequence<=eventSequence||s.us<eventUs||s.us-eventUs>observeUs)return;
        auto& side=s.id==driver?v:npc;
        if(side.measured||side.delivered)return;
        const auto correction=contactCorrection(s,direction,s.id==driver);
        if(!correction.valid)return;
        side.measured=true;side.sampleSequence=s.sequence;side.sample=s;side.adjustment=correction;
        if(s.id==driver)++vMeasured;else {++npcMeasured;if(s.physicsState==1)++trafficMeasured;}
    }
    template<std::size_t N> void seed(const std::array<TraceSample,N>& ring) {
        // Use the earliest eligible correction of each exact entity, not the
        // strongest nearby or a matching NPC/V sample.
        const TraceSample* earliestV=nullptr;const TraceSample* earliestNPC=nullptr;
        for(const auto& s:ring) {
            if(!includes(s.id)||s.sequence<=eventSequence||s.us<eventUs||s.us-eventUs>observeUs||
               !contactCorrection(s,direction,s.id==driver).valid)continue;
            auto*& first=s.id==driver?earliestV:earliestNPC;
            if(!first||s.us<first->us||(s.us==first->us&&s.sequence<first->sequence))first=&s;
        }
        if(earliestV)offer(*earliestV);
        if(earliestNPC)offer(*earliestNPC);
    }
    ContactAdjustment delivery(std::uint64_t id,std::int64_t stamp,unsigned state,bool candidate,
        float inverse,Vec linear,Vec angular,bool context) {
        expire(stamp);
        if(!includes(id))return {};
        if(!context){++contextCancelled;close("context");return {};}
        auto& side=id==driver?v:npc;
        if(!side.measured||side.delivered)return {};
        // No write to traffic, parked, asleep, kinematic or scene state.
        // The vanilla transition can make a measured traffic body dynamic.
        if(state==1&&id==target)return {};
        if(state!=0||candidate!=(id==driver)||!std::isfinite(inverse)||inverse<=0||
           std::abs(inverse-side.sample.inverseMass)>side.sample.inverseMass*.05f||
           !valid(linear)||!valid(angular)||
           length(sub(linear,side.sample.after))>8.f||
           length(add(linear,side.adjustment.linear))>100.f||
           length(add(angular,side.adjustment.angular))>35.f) {
            ++writeRejected;side.delivered=true;
            if(v.delivered&&npc.delivered)close("delivery-rejected");
            return {};
        }
        return side.adjustment;
    }
    void record(std::uint64_t id) {
        if(!includes(id))return;
        auto& side=id==driver?v:npc;
        if(side.delivered)return;
        side.delivered=true;if(id==driver)++vWrites;else ++npcWrites;
        if(v.delivered&&npc.delivered)close("delivered");
    }
};
}
