#pragma once
#include <cstdint>
namespace kineticle {
enum Protection : std::uint32_t { Mechanical=1, Grid=2, Glass=4 };
constexpr std::uint32_t protectionMask=Mechanical|Grid|Glass;
constexpr bool validProtectionFlags(std::uint32_t value) {return (value&~protectionMask)==0;}
enum class Scope : unsigned { Allowed, NotRoad, Driver, Remote, Quest, Destroyed, Locked, Player, Mounted, Blackboard, Tags, Count };
constexpr bool allowedScope(bool road,int driver,int remote,int quest,int destroyed,int locked,
    bool sameMounted,int psm,int noDriving,int forwardOnly,int scene) {
    return road&&driver==1&&remote==0&&quest==0&&destroyed==0&&locked==0&&sameMounted&&
        (psm==1||psm==6)&&noDriving==0&&forwardOnly==0&&scene==0;
}
}
