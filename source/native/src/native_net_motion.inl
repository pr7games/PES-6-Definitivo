#include "graphics_effect_options.h"
namespace {
struct NativeNetRest {uint32_t source=0;std::array<std::array<float,3>,28> p{};};
std::mutex nativeNetMutex;
std::map<uint32_t,NativeNetRest> nativeNetRest;
float NativeNetFloat(uint8_t* base,uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));}
void NativeNetStore(uint8_t* base,uint32_t at,float f){REX_STORE_U32(at,std::bit_cast<uint32_t>(f));}
}
extern "C" void pes6_native_net_rest(uint8_t* base,uint32_t model){
 if(!model)return;const auto descriptor=model+20,source=REX_LOAD_U32(descriptor);
 if(!source||REX_LOAD_U32(descriptor+4)!=7)return;
 NativeNetRest rest;rest.source=source;
 for(unsigned i=0;i<28;i++)for(unsigned j=0;j<3;j++)rest.p[i][j]=NativeNetFloat(base,source+i*20+j*4);
 std::lock_guard lock(nativeNetMutex);if(nativeNetRest.size()>16)nativeNetRest.clear();nativeNetRest[descriptor]=rest;
}
extern "C" void pes6_native_net_upload(PPCContext& ctx,uint8_t* base){
 if(!pes6::graphics::EffectOptions().particles){sub_82195A80(ctx,base);return;}
 const auto descriptor=ctx.r4.u32;std::unique_lock lock(nativeNetMutex);const auto it=nativeNetRest.find(descriptor);
 if(it==nativeNetRest.end()||it->second.source!=REX_LOAD_U32(descriptor)){lock.unlock();sub_82195A80(ctx,base);return;}
 const auto& rest=it->second;std::array<std::array<uint32_t,3>,28> original{};
 // Amplify only the native deformation. Undisplaced supports stay fixed;
 // native timing, impact location and replay/seek remain authoritative.
 for(unsigned i=0;i<28;i++)for(unsigned j=0;j<3;j++){const auto at=rest.source+i*20+j*4;original[i][j]=REX_LOAD_U32(at);const float p=std::bit_cast<float>(original[i][j]),r=rest.p[i][j];if(std::isfinite(p)&&std::isfinite(r))NativeNetStore(base,at,r+(p-r)*(j==1?1.25f:1.45f));}
 sub_82195A80(ctx,base);
 for(unsigned i=0;i<28;i++)for(unsigned j=0;j<3;j++)REX_STORE_U32(rest.source+i*20+j*4,original[i][j]);
}
extern "C" void pes6_wembley_crowd_color(PPCContext& ctx,uint8_t* base,bool detailed){
 const auto config=REX_LOAD_U32(0x83139360u);if(!config||base[config+130]!=10)return;
 // Imported crowd normals can feed the native fully shaded endpoint. Keep
 // at least75% of the crowd texture, rather than the old20% floor. Texture
 // skin/clothing detail, native alpha and the other stadiums remain intact.
 // Cache the curve once instead of evaluating pow per crowd vertex/channel.
 static const auto curve=[](){std::array<uint8_t,256> values{};for(unsigned i=0;i<256;i++)values[i]=uint8_t(std::lround(192.f+63.f*std::pow(i/255.f,.72f)));return values;}();
 const auto lift=[](uint32_t c){uint32_t result=c&0xff000000u;for(unsigned shift:{0u,8u,16u})result|=uint32_t(curve[(c>>shift)&255])<<shift;return result;};
 if(detailed){ctx.r6.u64=lift(ctx.r6.u32);ctx.r7.u64=lift(ctx.r7.u32);}else ctx.r5.u64=lift(ctx.r5.u32);
}
