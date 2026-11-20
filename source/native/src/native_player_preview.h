#pragma once
#include <cstdint>
#include <string>
struct PPCContext;
namespace pes6::editor::preview {
void Enter(PPCContext&,uint8_t*,uint16_t,int team=-1,unsigned variant=0);
void SwitchPlayer(uint16_t);
void Leave(PPCContext&,uint8_t*);
void Tick(PPCContext&,uint8_t*);
void Changed(PPCContext&,uint8_t*,bool rebuild=false);
void Animation(PPCContext&,uint8_t*,const std::string&);
void Container(uint32_t);
void Visible(bool);
void Frame(int); // 0 full body, 1 upper body, 2 lower body, 3 info, 4 face/hair
void Rotate(int);
void Rest();
void TeamKit(PPCContext&,uint8_t*,unsigned team,unsigned variant);
bool Ready();
const char* Status();
}
