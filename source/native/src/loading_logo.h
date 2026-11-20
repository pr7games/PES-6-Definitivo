#pragma once
// Full atlas variants differ ONLY inside the former RBK UV rectangle.
// Recognize exact source pixels before loading; never replace by dimensions alone.
namespace pes6_loading_logo {
inline std::array<std::atomic<uint32_t>,24> textures{};
inline std::array<std::atomic<uint32_t>,16> originals{};
inline std::array<std::atomic<uint32_t>,16> gpus{};
inline std::atomic<bool> loading{false};
inline bool attempted=false;
inline std::mutex mutex;
inline bool Source(uint8_t* base,uint32_t source){
  if(std::memcmp(base+source,"WE00",4)||(base[source+7]!=8&&base[source+7]!=32)||
     REX_LOAD_U16(source+8)!=256||REX_LOAD_U16(source+10)!=256)return false;
  const unsigned size=base[source+7]==8?66576:262160;
  if(!MenuReadable(base,source,size))return false;
  uint64_t hash=14695981039346656037ull;
  for(unsigned i=0;i<size;i++)hash=(hash^base[source+i])*1099511628211ull;
  return hash==0xf7d2ecba818a1342ull||hash==0xd3c4dd53665870a5ull;
}
inline void Loaded(PPCContext& ctx,uint8_t* base,uint32_t source,bool recognized=false){
  if(loading||!ctx.r3.u32||(!recognized&&(!MenuReadable(base,source,16)||!Source(base,source))))return;
  std::lock_guard lock(mutex);
  const uint32_t original=ctx.r3.u32;
  for(auto& slot:originals)if(slot==original||!slot){slot=original;break;}
  if(attempted)return;
  attempted=true;loading=true;
  const auto root=MenuCarouselRoot()/"data/loading-logo";
  // Prepare once, not per draw. Keep texture references for process lifetime.
  for(unsigned i=0;i<textures.size();i++){
    const auto name=(i<10?"0":"")+std::to_string(i)+".we";
    std::ifstream in(root/name,std::ios::binary|std::ios::ate);
    if(!in||in.tellg()!=262160)break;
    PPCContext alloc=ctx;alloc.r3.u64=262224;sub_826D61B0(alloc,base);
    const auto data=alloc.r3.u32;if(!data)break;
    in.seekg(0);in.read(reinterpret_cast<char*>(base+data),262160);
    if(in&&std::memcmp(base+data,"WE00",4)==0&&base[data+7]==32&&
       REX_LOAD_U16(data+8)==256&&REX_LOAD_U16(data+10)==256){
      const auto label=std::string("golazo_loading_")+std::to_string(i);
      std::memcpy(base+data+262160,label.c_str(),label.size()+1);
      PPCContext load=ctx;load.r3.u64=data;load.r4.u64=data+262160;
      sub_82736390(load,base);textures[i]=load.r3.u32;
      if(load.r3.u32){
        const auto gpu=REX_LOAD_U32(load.r3.u32+36);
        if(gpu){PPCContext filter=ctx;filter.r3.u64=gpu;filter.r4.u64=0;filter.r5.u64=3;sub_8218FB18(filter,base);}
      }
    }
    PPCContext release=ctx;release.r3.u64=data;sub_8280E7B8(release,base);
    if(!textures[i])break;
  }
  loading=false;
}
inline void GpuLoaded(PPCContext& ctx,uint8_t* base,bool recognized){
  if(!recognized||loading||!ctx.r3.u32)return;
  const uint32_t gpu=ctx.r3.u32;
  for(auto& slot:gpus)if(slot==gpu||!slot){slot=gpu;break;}
  // Prepare frames even when the loading screen bypasses GUI texture objects.
  Loaded(ctx,base,0,true);
}
inline void Bind(PPCContext& ctx,uint8_t* base,uint64_t now){
  if(loading||ctx.r3.u32||!ctx.r4.u32)return;
  for(const auto& slot:gpus){const auto gpu=slot.load();if(!gpu||REX_LOAD_U32(gpu+20)!=ctx.r4.u32)continue;
    const auto frame=textures[unsigned(now%2400)/100].load();
    const auto target=frame?REX_LOAD_U32(frame+36):0;
    if(target){const auto handle=REX_LOAD_U32(target+20);if(handle)ctx.r4.u64=handle;}
    return;
  }
}
inline void GpuReleased(uint32_t gpu){
  if(!gpu)return;
  for(auto& slot:gpus)if(slot==gpu)slot=0;
}
inline uint32_t Draw(uint32_t texture,uint64_t now){
  if(!texture||loading)return texture;
  for(const auto& original:originals)if(original.load()==texture){
    const auto frame=unsigned(now%2400)/100;
    const auto selected=textures[frame].load(),first=textures[0].load();
    return selected?selected:(first?first:texture);
  }
  return texture;
}
inline void Released(uint8_t* base,uint32_t texture){
  if(!texture)return;
  for(auto& original:originals)if(original==texture&&REX_LOAD_U32(texture+40)<=1)original=0;
}
}
