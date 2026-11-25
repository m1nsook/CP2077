#pragma once
#include "physics.hpp"
#include <array>
#include <cstdint>
#include <cstddef>

namespace kineticle {
// Bounded value-only trace, caller serializes access. Keep a narrow window
// around a reported pair; never use these temporal matches to authorize writes.
struct TraceSample {
    std::uint64_t id{},sequence{};
    std::int64_t us{};
    std::uint32_t thread{};
    Vec before{},after{},angularBefore{},angularAfter{};
    float loss{},change{};
};
struct TraceEvent {
    std::uint64_t driver{},target{};
    std::int64_t us{};
    std::uint64_t sequence{};
    float delta{};
    TraceSample driverPre{},driverPost{},targetPre{},targetPost{};
};
struct ContactTrace {
    static constexpr std::int64_t windowUs=150000;
    std::array<TraceSample,512> samples{};
    std::array<TraceEvent,4> events{};
    std::size_t nextSample{},nextEvent{};
    static void match(TraceEvent& e,const TraceSample& s) {
        if(!e.driver||!s.id||(s.id!=e.driver&&s.id!=e.target))return;
        auto dt=s.us-e.us;
        if(dt < -windowUs||dt > windowUs)return;
        auto& dst=s.id==e.driver?(dt<=0?e.driverPre:e.driverPost):(dt<=0?e.targetPre:e.targetPost);
        if(!dst.id||s.change>dst.change)dst=s;
    }
    void push(TraceSample s) {
        if(!s.id)return;
        samples[nextSample++%samples.size()]=s;
        for(auto& e:events)match(e,s);
    }
    void event(std::uint64_t driver,std::uint64_t target,std::int64_t us,std::uint64_t seq,float delta) {
        if(!driver||!target||driver==target)return;
        auto& e=events[nextEvent++%events.size()];
        e={driver,target,us,seq,delta};
        for(const auto& s:samples)match(e,s);
    }
};
}
