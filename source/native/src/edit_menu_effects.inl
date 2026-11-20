// Effects only: no main-menu background, logo or player artwork.
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <vector>
extern "C" void pes6_edit_effect_geometry(PPCContext&,uint8_t*,uint32_t);
namespace {
struct EditEffects {
  uint32_t transition=0,ambient=0,foreground=0,screen=0,resource=0;
  uint64_t wave_until=0;
  bool requested=false,restart=false;
} edit_effects;
// Photo identity survives disposal/recreation of the surrounding menu scene.
uint32_t edit_last_picture=0;
std::map<uint32_t,uint32_t> edit_effect_geometry;
std::unordered_set<uint32_t> edit_effect_nodes;
struct EditFxFrame{std::array<uint8_t,24> header;std::vector<uint8_t> pointers;std::vector<unsigned> local_slots;};
std::unordered_map<uint32_t,EditFxFrame> edit_effect_frames;
std::vector<uint32_t> edit_retired_geometry;
unsigned edit_retired_resource_refs=0;
uint32_t EditFxCall(PPCContext& context,uint8_t* base,PPCFunc* fn,uint32_t a=0,uint32_t b=0,uint32_t c=0,uint32_t d=0){
  PPCContext call=context;call.r3.u64=a;call.r4.u64=b;call.r5.u64=c;call.r6.u64=d;fn(call,base);return call.r3.u32;
}
void EditFxAnimation(PPCContext& context,uint8_t* base,uint32_t node,unsigned last,bool loop){
  if(!node)return;
  PPCContext call=context;call.r3.u64=node;call.f1.f64=1.f;sub_8272C000(call,base);
  EditFxCall(context,base,sub_8272C080,node,1);EditFxCall(context,base,sub_8272C088,node,last);
  EditFxCall(context,base,sub_8272C090,node,loop?1:0);EditFxCall(context,base,sub_8272C0A0,node,1);
}
void EditFxFilterTree(PPCContext& ctx,uint8_t* base,uint32_t node,unsigned depth=0){
  if(depth>4||!MenuReadable(base,node,260))return;
  edit_effect_nodes.insert(node);
  pes6_edit_effect_geometry(ctx,base,node);
  // Wallpaper is drawn in the normal scene pass; effects follow in queue1.
  // Editing controls and portraits are explicitly promoted to queue2.
  REX_STORE_U32(node+68,1);
  base[node+52]|=0x23;
  const auto begin=REX_LOAD_U32(node+248),end=REX_LOAD_U32(node+252);
  if(end<begin||end-begin>256||!MenuReadable(base,begin,end-begin))return;
  for(auto at=begin;at<end;at+=4){
    const auto child=REX_LOAD_U32(at);
    if(MenuReadable(base,child,132)&&REX_LOAD_U32(child+128)==REX_LOAD_U32(node+128))
      EditFxFilterTree(ctx,base,child,depth+1);
  }
}
// Reorder existing list links without deleting/recreating native UI objects.
// The gold background stays below the effects and all edit controls above them.
void EditFxBefore(uint8_t* base,uint32_t object,uint32_t foreground){
  const auto list=REX_LOAD_U32(0x82d90000u-21328u);
  if(!MenuReadable(base,list,8))return;
  const auto sentinel=REX_LOAD_U32(list+4);
  if(!MenuReadable(base,sentinel,12))return;
  uint32_t source=0,target=0;
  auto link=REX_LOAD_U32(sentinel);
  for(unsigned i=0;i<2048&&link!=sentinel&&MenuReadable(base,link,12);i++,link=REX_LOAD_U32(link)){
    const auto value=REX_LOAD_U32(link+8);
    if(value==object)source=link;if(value==foreground)target=link;
  }
  if(!source||!target||source==target||REX_LOAD_U32(source)==target)return;
  const auto next=REX_LOAD_U32(source),previous=REX_LOAD_U32(source+4),before=REX_LOAD_U32(target+4);
  if(!MenuReadable(base,next,8)||!MenuReadable(base,previous,8)||!MenuReadable(base,before,8))return;
  REX_STORE_U32(previous,next);REX_STORE_U32(next+4,previous);
  const auto insertion=REX_LOAD_U32(target+4);
  REX_STORE_U32(source,target);REX_STORE_U32(source+4,insertion);
  REX_STORE_U32(insertion,source);REX_STORE_U32(target+4,source);
}
}
// Native setters assume every supplied pointer owns a valid child list.
// Selector fields can also contain text/controllers during screen changes.
namespace {
void EditForegroundTree(uint8_t* base,uint32_t node,unsigned depth) {
  if(depth>32||!MenuReadable(base,node,72))return;
  const auto vtable=REX_LOAD_U32(node);
  if(vtable<0x82000000u||vtable>=0x83000000u||!MenuReadable(base,vtable,28))return;
  const auto setter=REX_LOAD_U32(vtable+24);
  if(setter!=0x8272c7c0u&&setter!=0x82735c50u)return;
  REX_STORE_U32(node+68,2);
  if(setter!=0x8272c7c0u||!MenuReadable(base,node,224))return;
  const auto sentinel=REX_LOAD_U32(node+220);
  if(!MenuReadable(base,sentinel,12))return;
  auto link=REX_LOAD_U32(sentinel);
  for(unsigned i=0;i<2048&&link!=sentinel&&MenuReadable(base,link,12);i++) {
    EditForegroundTree(base,REX_LOAD_U32(link+8),depth+1);
    const auto next=REX_LOAD_U32(link);
    if(next==link)break;
    link=next;
  }
}
}
extern "C" void pes6_edit_foreground_tree(uint8_t* base,uint32_t node) {
  EditForegroundTree(base,node,0);
}
extern "C" void pes6_edit_effects(PPCContext& ctx,uint8_t* base,uint32_t foreground,uint32_t screen){
  auto& e=edit_effects;
  if(!foreground){
    if(e.transition)base[e.transition+53]=0;if(e.ambient)base[e.ambient+53]=0;e.screen=0;e.foreground=0;return;
  }
  // 3D models and screens without portraits are not picture changes.
  const bool picture=(screen&0xf0000000u)!=0x50000000u&&screen!=0x4ffu;
  if(e.screen!=screen){e.screen=screen;e.restart=picture&&edit_last_picture!=screen;}
  if(picture)edit_last_picture=screen;
  if(MenuReadable(base,foreground,224)&&REX_LOAD_U32(foreground+68)!=2)
    pes6_edit_foreground_tree(base,foreground);
  if(!e.requested){EditFxCall(ctx,base,sub_82729290,3091,1);e.requested=true;}
  if(!e.transition||!e.ambient){
    if(EditFxCall(ctx,base,sub_82729350,3091)!=2)return;
    const auto resource=EditFxCall(ctx,base,sub_8272A848,0x820060c4u);
    if(!resource)return;
    e.resource=resource;
    // Only the whitelisted particle/light meshes of animation0 are retained
    // by the geometry hook below. No static background, logo or portrait.
    if(!e.ambient){
      e.ambient=EditFxCall(ctx,base,sub_8272A388,resource,0,0,65535);
      EditFxAnimation(ctx,base,e.ambient,450,true);
    }
    if(!e.transition){
      e.transition=EditFxCall(ctx,base,sub_8272FCB0,resource,15,0,65535);
      if(e.transition){
        const auto children=REX_LOAD_U32(e.transition+248);
        if(MenuReadable(base,children,4))EditFxAnimation(ctx,base,REX_LOAD_U32(children),450,true);
      }
    }
    // Filter before the first submitted frame as well as subsequent updates.
    EditFxFilterTree(ctx,base,e.ambient);EditFxFilterTree(ctx,base,e.transition);
    e.foreground=0;
  }
  if(e.transition)base[e.transition+53]=1;
  if(e.ambient)base[e.ambient+53]=1;
  if(e.foreground!=foreground){
    EditFxBefore(base,e.ambient,foreground);EditFxBefore(base,e.transition,foreground);e.foreground=foreground;
  }
  if(e.restart&&e.transition){
    EditFxAnimation(ctx,base,e.transition,40,false);
    e.wave_until=GetTickCount64()+667;
    e.restart=false;
  }
  // End every entrance after 40 frames; ambient effects continue looping.
  if(e.transition)base[e.transition+53]=picture&&GetTickCount64()<e.wave_until;
}
extern "C" void pes6_edit_effect_geometry(PPCContext& ctx,uint8_t* base,uint32_t node){
  auto& e=edit_effects;
  if(!e.transition&&!e.ambient)return;
  // Most nodes were registered before the first frame. A native animation
  // may attach another child later: reject other resources in one read, then
  // validate ancestry only for new nodes from our resource.
  if(edit_effect_nodes.find(node)==edit_effect_nodes.end()){
    if(!e.resource||REX_LOAD_U32(node+128)!=e.resource)return;
    bool owned=false;auto ancestor=node;
    for(unsigned depth=0;depth<16&&MenuReadable(base,ancestor,68);depth++){
      if(ancestor==e.transition||ancestor==e.ambient){owned=true;break;}
      ancestor=REX_LOAD_U32(ancestor+64);
    }
    if(!owned)return;
    edit_effect_nodes.insert(node);
  }
  REX_STORE_U32(node+68,1);
  const auto source=REX_LOAD_U32(node+140);
  auto& scratch=edit_effect_geometry[node];
  if(source==scratch)return;
  // Animation frame descriptors live in the immutable menu resource. Filter
  // each once, then copy the compact descriptor without inspecting its meshes
  // or querying Windows memory again every time the ambient loop repeats.
  if(const auto frame=edit_effect_frames.find(source);scratch&&frame!=edit_effect_frames.end()){
    std::memcpy(base+scratch,frame->second.header.data(),24);
    REX_STORE_U32(scratch+12,scratch+32);
    if(!frame->second.pointers.empty())
      std::memcpy(base+scratch+32,frame->second.pointers.data(),frame->second.pointers.size());
    const auto& header=frame->second.header;
    auto local=scratch+32+((unsigned(header[0])<<8)|header[1])*4;
    for(auto slot:frame->second.local_slots){REX_STORE_U32(scratch+32+slot*4,local);local+=28;}
    REX_STORE_U32(node+140,scratch);return;
  }
  if(!MenuReadable(base,source,24))return;
  const unsigned count=REX_LOAD_U16(source);const auto table=REX_LOAD_U32(source+12);
  if(count>128||!MenuReadable(base,table,count*4))return;
  if(!scratch)scratch=EditFxCall(ctx,base,sub_826D61B0,8192);
  if(!scratch){if(e.transition)base[e.transition+53]=0;if(e.ambient)base[e.ambient+53]=0;return;}
  const auto target=scratch,pointers=target+32;auto cursor=pointers+count*4;
  EditFxFrame frame;
  std::memcpy(base+target,base+source,24);REX_STORE_U32(target+12,pointers);
  for(unsigned i=0;i<count;i++){
    const auto mesh=REX_LOAD_U32(table+i*4);if(!MenuReadable(base,mesh,28))return;
    REX_STORE_U32(pointers+i*4,mesh);
    // Animated streaks/lines (2), light rays (5), glints/particles (1). The same
    // group ALSO contains the PES logo (4) and an opaque backdrop (6).
    bool effect=base[mesh+3]==2||base[mesh+3]==5;
    if(base[mesh+3]==1){
      const unsigned vertices=REX_LOAD_U16(mesh+14);const auto points=REX_LOAD_U32(mesh+16);
      if(vertices&&vertices<=256&&MenuReadable(base,points,vertices*6)){
        int minx=32767,miny=32767,maxx=-32768,maxy=-32768;
        for(unsigned v=0;v<vertices;v++){
          const int x=int16_t(REX_LOAD_U16(points+v*6)),y=int16_t(REX_LOAD_U16(points+v*6+2));
          minx=std::min(minx,x);maxx=std::max(maxx,x);miny=std::min(miny,y);maxy=std::max(maxy,y);
        }
        effect=maxx-minx<=160*16&&maxy-miny<=160*16;
      }
    }
    // Native geometry setup expects the original slot count/order, including
    // disabled placeholders. Never compact this animation descriptor to zero.
    if(!effect){std::memcpy(base+cursor,base+mesh,28);base[cursor]=0;REX_STORE_U32(pointers+i*4,cursor);cursor+=28;frame.local_slots.push_back(i);}
  }
  std::memcpy(frame.header.data(),base+target,24);
  // Include disabled placeholders and relocate their table slots on reuse.
  // Keep the exact native count/order, including entirely disabled frames.
  frame.pointers.assign(base+pointers,base+cursor);
  edit_effect_frames.emplace(source,std::move(frame));
  REX_STORE_U32(node+140,target);
}
extern "C" void pes6_edit_text_foreground(uint8_t* base,uint32_t node){
  if(edit_effects.foreground&&MenuReadable(base,node,72))REX_STORE_U32(node+68,2);
}
extern "C" void pes6_edit_effects_close(PPCContext& ctx,uint8_t* base){
  auto& e=edit_effects;
  for(auto node:{e.transition,e.ambient})if(MenuReadable(base,node,4)){
    base[node+53]=0;
    const auto table=REX_LOAD_U32(node);
    if(MenuReadable(base,table,16))if(auto* fn=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(table+12)))EditFxCall(ctx,base,fn,node);
  }
  // Root disposal is deferred by 82735EC8. Their geometry and resource must
  // stay alive until the native deletion queue has actually run.
  for(const auto& entry:edit_effect_geometry)if(entry.second)edit_retired_geometry.push_back(entry.second);
  edit_effect_geometry.clear();
  if(e.requested)++edit_retired_resource_refs;
  edit_effect_nodes.clear();
  edit_effect_frames.clear();
  e={};
}
extern "C" void pes6_edit_effects_collected(PPCContext& ctx,uint8_t* base){
  // Called AFTER native 82738BF0 destroys all queued UI roots, on every menu.
  for(auto memory:edit_retired_geometry)EditFxCall(ctx,base,sub_8280E7B8,memory);
  edit_retired_geometry.clear();
  while(edit_retired_resource_refs){--edit_retired_resource_refs;EditFxCall(ctx,base,sub_827292E0,3091);}
}
