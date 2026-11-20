#include <iomanip>
#include <sstream>
#include <map>
namespace {
struct StadiumEdit {std::string name;uint32_t capacity=0,year=0,nameAddress=0;int pattern=-1;int64_t net=-1;};
std::array<StadiumEdit,9> stadiumEdits;
void LoadStadiumEdits(){
 static std::once_flag once;std::call_once(once,[]{wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::ifstream in(root/"data/editor/stadiums.txt",std::ios::binary);std::string line;while(std::getline(in,line)){std::istringstream row(line);unsigned id,capacity,year;int pattern;int64_t net;std::string name;if((row>>id>>std::quoted(name)>>capacity>>year>>pattern>>net)&&id<9&&!name.empty()&&name.size()<=63&&capacity<=250000&&(year==0||(year>=1800&&year<=2100))&&(pattern==-1||(pattern>=0&&pattern<8)||(pattern>=100&&pattern<120&&pattern!=110))&&net>=-1&&net<=0xffffff)stadiumEdits[id]={name,capacity,year,0,pattern,net};}});
}
bool EditedPitch(uint8_t* base){LoadStadiumEdits();const auto c=REX_LOAD_U32(0x83139360u);return c&&base[c+130]<9&&stadiumEdits[base[c+130]].pattern>=0&&stadiumEdits[base[c+130]].pattern<8;}
void ApplyStadiumEdits(PPCContext& ctx,uint8_t* base,uint32_t table){LoadStadiumEdits();for(unsigned id=0;id<9;id++){auto& x=stadiumEdits[id];if(x.capacity)REX_STORE_U32(table+32+id*12+4,x.capacity);if(x.year)REX_STORE_U32(table+32+id*12+8,x.year);if(!x.name.empty()&&!x.nameAddress){PPCContext a=ctx;a.r3.u64=x.name.size()+1;sub_826D61B0(a,base);if(a.r3.u32){x.nameAddress=a.r3.u32;std::memcpy(base+x.nameAddress,x.name.c_str(),x.name.size()+1);}}if(id<8&&x.nameAddress)REX_STORE_U32(table+208+id*4,x.nameAddress);}}
}
extern "C" void pes6_stadium_edit_net(uint8_t* base,uint32_t net){
 LoadStadiumEdits();const auto c=REX_LOAD_U32(0x83139360u);if(!c||base[c+130]>=9||!net)return;const auto model=REX_LOAD_U32(net+84);if(!model)return;
 // 82170030 packs ARGB; 8216FEC0/82197848 copy it into the two native
 // net render descriptors. Keep all native geometry and deformation intact.
 const auto env=REX_LOAD_U32(0x831393A4u);const unsigned offset=base[net+80]?68:62;int64_t color=stadiumEdits[base[c+130]].net;
 if(color<0){if(!env)return;color=(uint32_t(REX_LOAD_U16(env+offset)&255)<<16)|(uint32_t(REX_LOAD_U16(env+offset+2)&255)<<8)|(REX_LOAD_U16(env+offset+4)&255);}
 REX_STORE_U32(model+156,(REX_LOAD_U32(model+156)&0xff000000u)|uint32_t(color));
 REX_STORE_U32(model+36,(REX_LOAD_U32(model+36)&0xff000000u)|uint32_t(color));
}
