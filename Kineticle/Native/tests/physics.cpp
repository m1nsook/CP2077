#include "physics.hpp"
#include "contact_trace.hpp"
#include "report_pages.hpp"
#include "diagnostic_policy.hpp"
#include "motion_trial.hpp"
#include <cstdlib>
#include <cmath>
#include <limits>
using namespace kineticle;
void check(bool x){ if(!x) std::abort(); }
bool eq(float a,float b){return std::abs(a-b)<0.0001f;}
int main(){
    Vec a{20,0,2}, b{2,3,-1};
    check(eq(forwardLoss(a,b),18));
    auto x=recover(a,b,true,1);
    check(eq(x.x,20)&&eq(x.y,3)&&eq(x.z,-1));
    check(eq(recover(a,b,true,.5f).x,11));
    check(eq(recover(a,b,false,1).x,2));
    check(eq(recover(a,b,true,0).x,2));
    check(eq(recover(a,{25,0,0},true,1).x,25));
    check(eq(recover(a,b,true,std::numeric_limits<float>::quiet_NaN()).x,2));
    ContactTrace trace;
    trace.push({1,1,90000,0,{},{},{},{},1,1});
    trace.push({2,2,95000,0,{},{},{},{},0,2});
    trace.push({3,3,99000,0,{},{},{},{},0,99});
    trace.event(1,2,100000,4,1);
    trace.push({1,5,110000,0,{},{},{},{},3,3});
    trace.push({2,6,260000,0,{},{},{},{},0,99});
    trace.push({2,7,120000,0,{},{},{},{},0,4});
    const auto& e=trace.events[0];
    check(e.driverPre.sequence==1&&e.targetPre.sequence==2);
    check(e.driverPost.sequence==5&&e.targetPost.sequence==7);
    trace.push({1,8,130000,0,{},{},{},{},1,1});
    check(e.driverPost.sequence==5);
    trace={};
    check(trace.events[0].driver==0);
    std::string report;
    for(int i=0;i<500;++i)report+="; contact velocity=123.456";
    auto pages=reportPages(report);
    check(pages.size()>1);
    std::string reconstructed;
    for(std::size_t i=0;i<pages.size();++i){
        check(pages[i].size()<=reportPayloadBytes);
        check(numberedPage("0.1.0-dev.8-native-motion",pages,i).size()<=2048);
        reconstructed+=pages[i];
    }
    check(reconstructed==report);
    check(numberedPage("version",pages,pages.size()).empty());
    check(reportPages("").empty());
    auto longField=std::string(5000,'x');
    auto longPages=reportPages(longField);
    check(longPages.size()==3);
    reconstructed.clear();
    for(const auto& part:longPages)reconstructed+=part;
    check(reconstructed==longField);
    check(oneDriver(0,1)&&oneDriver(1,0)&&!oneDriver(-1,1)&&!oneDriver(1,1));
    check(nativeGuards(1,0,0,0,0,0,0,true,true,false));
    for(int i=0;i<5;++i) {
        int g[5]={0,0,0,0,0};g[i]=-1;
        check(!nativeGuards(1,0,g[0],g[1],g[2],g[3],g[4],true,true,false));
        g[i]=1;
        check(!nativeGuards(1,0,g[0],g[1],g[2],g[3],g[4],true,true,false));
    }
    check(!nativeGuards(1,0,0,0,0,0,0,true,false,false));
    check(!nativeGuards(1,0,0,0,0,0,0,true,true,true));
    check(nearPair(1,2,15000,2,1,10000));
    check(!nearPair(1,2,65001,2,1,10000)&&!nearPair(1,2,15000,1,3,10000));
    check(validMass(1500,1.0f/1500)&&!validMass(0,0)&&!validMass(1000,-1));
    auto est=estimate({10,0,0},{5,0,0},1000,true);
    check(eq(est.impulse,5000)&&eq(est.extra5,20000)&&eq(est.extra100,495000));
    check(eq(est.recover50.x,7.5f)&&eq(est.recover100.x,10));
    check(estimate({10,0,0},{5,0,0},0,false).impulse==0);
    // Replay the equal-mass collision from dev.7, preserving the measured timing.
    TraceSample v{},npc{};
    v.id=9015084;v.sequence=100786;v.us=120859;v.candidate=true;v.mass=1600;v.inverseMass=.000625f;v.massOk=true;v.dt=.0333333f;
    v.before={1.33757f,25.0523f,-.348003f};v.after={1.01073f,19.6988f,-.0974044f};
    v.change=length(sub(v.after,v.before));v.loss=forwardLoss(v.before,v.after);
    npc.id=10564884;npc.sequence=100788;npc.us=120879;npc.mass=1600;npc.inverseMass=.000625f;npc.massOk=true;npc.dt=v.dt;
    npc.before={.923578f,15.2854f,.331723f};npc.after={1.25042f,20.6389f,.0811251f};
    npc.change=length(sub(npc.after,npc.before));npc.loss=forwardLoss(npc.before,npc.after);
    const auto motion=planMotion(v,npc);
    check(motion.reason==TrialReject::None&&motion.error<.001f&&motion.driverAdd.z==0&&motion.targetAdd.z==0);
    check(length(motion.driverAdd)>2.6f&&length(motion.driverAdd)<2.8f);
    check(length(motion.targetAdd)>5.3f&&length(motion.targetAdd)<5.4f);
    auto unrelated=npc;unrelated.after=npc.before;check(planMotion(v,unrelated).reason==TrialReject::Momentum);
    auto flagged=npc;flagged.candidate=true;check(planMotion(v,flagged).reason==TrialReject::Sample);
    auto traffic=npc;traffic.physicsState=1;check(planMotion(v,traffic).reason==TrialReject::Sample);
    auto late=npc;late.us+=6000;check(planMotion(v,late).reason==TrialReject::Timing);
    MotionTrial mt;mt.arm(v.id,npc.id,100000,100751);
    std::array<TraceSample,2> source{npc,v};mt.seed(source);
    check(mt.ready&&mt.planned==1);
    auto cleanNpc=npc;cleanNpc.sequence+=10;cleanNpc.us+=30000;cleanNpc.before=cleanNpc.after;cleanNpc.change=0;
    auto cleanV=v;cleanV.sequence+=20;cleanV.us+=31000;cleanV.before=cleanV.after;cleanV.change=0;
    check(!mt.step(cleanV,true).apply); // target must be delivered first
    auto act=mt.step(cleanNpc,true);check(act.apply&&!act.driver);
    mt.recordWrite(act,cleanNpc.after,add(cleanNpc.after,act.delta),cleanNpc.us);
    check(!mt.step(cleanNpc,true).apply);
    act=mt.step(cleanV,true);check(act.apply&&act.driver);
    mt.recordWrite(act,cleanV.after,add(cleanV.after,act.delta),cleanV.us);
    check(!mt.active&&mt.targetWrites==1&&mt.driverWrites==1&&mt.completed==1);
    mt.arm(v.id,npc.id,100000,100751);mt.seed(source);
    auto wall=cleanNpc;wall.change=1;check(!mt.step(wall,true).apply&&!mt.active&&mt.lastReason==TrialReject::NewCorrection);
    mt.arm(v.id,npc.id,100000,100751);mt.seed(source);
    check(!mt.step(cleanNpc,false).apply&&!mt.active&&mt.lastReason==TrialReject::Context);
    mt.arm(v.id,npc.id,100000,100751);mt.seed(source);mt.expire(220001);
    check(!mt.active&&mt.lastReason==TrialReject::Expired);
    mt.arm(v.id,npc.id,100000,100751);mt.seed(source);
    act=mt.step(cleanNpc,true);check(act.apply);
    mt.recordWrite(act,cleanNpc.after,add(cleanNpc.after,act.delta),cleanNpc.us);
    mt.cancel(TrialReject::Overlap);check(mt.partial==1&&!mt.active);
    mt.arm(v.id,npc.id,100000,100751);
    auto pre=v;pre.us=99000;mt.offer(pre);check(!mt.d.id);
    auto stray=v;stray.id=42;mt.offer(stray);check(!mt.d.id);
    mt.offer(v);mt.offer(npc);mt.finishPlan();check(mt.ready);
}
