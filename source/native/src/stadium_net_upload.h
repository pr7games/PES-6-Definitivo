#pragma once
#include <cstdint>
#include <vector>
#include <set>
#include <algorithm>

namespace pes6::stadium_net {
struct PositionStream {uint32_t descriptor,source,bytes;};
struct Upload {uint32_t resource,source,bytes;};
// Native 82205218 creates separate render resources for each packet's vertex
// stream and copies its source through resource.vtable[6]. 82201370 only
// relocates the model header; editing its referenced source is not an upload.
template<class Word,class Byte>
std::vector<Upload> ResolveUploads(uint32_t model,const std::vector<PositionStream>& streams,Word word,Byte byte){
 std::vector<Upload> result;std::set<uint32_t> seen;
 for(unsigned list:{20u,36u}){
  const auto first=word(model+list),end=word(model+list+4);
  if(!first||end<first||(end-first)%4||(end-first)/4>256)continue;
  for(auto at=first;at<end;at+=4){
   const auto packet=word(at);if(!packet)continue;
   const auto sourcePacket=word(packet),resources=word(packet+12);if(!sourcePacket||!resources)continue;
   const auto count=word(sourcePacket+16);if(count>256)continue;
   for(unsigned i=0;i<count;i++){
    const auto descriptor=sourcePacket+word(sourcePacket+20)+i*32;
    const auto stream=std::find_if(streams.begin(),streams.end(),[&](const auto& s){return s.descriptor==descriptor;});
    // Imported triangle-list nets have one buffer per stream. Do not assume
    // the layout of animated multi-buffer resources from unrelated models.
    if(stream==streams.end()||byte(descriptor+8)!=1)continue;
    const auto copies=word(resources+i*4),resource=copies?word(copies):0;
    if(resource&&seen.insert(resource).second)result.push_back({resource,stream->source,stream->bytes});
   }
  }
 }
 return result;
}
}
