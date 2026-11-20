#pragma once
#include <cstdint>
#include <cstddef>

namespace pes6::weather {
// Query the runtime's current guest page table, including the entire range.
// This deliberately does not cache readability across frees/protection changes.
template<class Memory,class RegionInfo>
bool ReadableGuestRange(Memory& memory,const uint8_t* base,uint32_t address,size_t bytes,RegionInfo& info){
  const uint64_t end=uint64_t(address)+bytes;
  if(!bytes||address<0x10000||end>=0xe0000000ull||end<=address||memory.virtual_membase()!=base)return false;
  auto* heap=memory.LookupHeap(address);
  if(!heap||memory.LookupHeap(uint32_t(end-1))!=heap)return false;
  // QueryRangeAccess checks protection only. Decommit retains current_protect
  // in this runtime, so a readable bit alone does NOT mean the page exists.
  if(!(unsigned(heap->QueryRangeAccess(address,uint32_t(end-1)))&1u))return false;
  uint64_t cursor=address;
  while(cursor<end){
    if(!heap->QueryRegionInfo(uint32_t(cursor),&info)||(info.state&2u)==0||(info.protect&1u)==0)return false;
    const uint64_t page=heap->heap_base()+((cursor-heap->heap_base())/heap->page_size())*heap->page_size();
    const uint64_t next=page+info.region_size;
    if(next<=cursor)return false;
    cursor=next;
  }
  return true;
}
}
