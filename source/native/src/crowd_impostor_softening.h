#pragma once

// Filter the crowd's own RGBA representation. No camera/scene DOF state.
inline bool SoftCrowdImpostors(){
 static const bool enabled=[](){const auto* v=std::getenv("PES6_SOFT_CROWD");return v&&v[0]=='1'&&v[1]==0;}();
 return enabled;
}
inline void SoftCrowdAtlasSize(PPCContext& ctx){
 if(!SoftCrowdImpostors())return;
 // 821614E8 recomputes texture dimensions and native atlas packing from these
 // inputs. Restrict the change to the two crowd targets from 8214E8F0.
 if(ctx.lr==0x8214E97Cu&&ctx.r4.u32==ctx.r5.u32){
  if(ctx.r4.u32==512)ctx.r4.u64=ctx.r5.u64=128;
  else if(ctx.r4.u32==128)ctx.r4.u64=ctx.r5.u64=64;
 }
}
inline void SoftCrowdImpostorMaterial(uint8_t* base,uint32_t object){
 if(!SoftCrowdImpostors())return;
 const uint32_t crowd=REX_LOAD_U32(0x831393E8u);
 if(!crowd||object!=crowd+276)return;
 // Preserve depth testing, but do not convert the sampled fractional alpha
 // back into a binary silhouette. Native blend mode0 is source-alpha blend.
 base[object+40]=0; // no depth writes from the transparent quad margins
 base[object+42]=0; // no alpha-test threshold
 base[object+52]=1;
 REX_STORE_U32(object+56,0);
 // The low-resolution RGBA silhouette must be reconstructed with linear
 // sampling, including its coverage/alpha, rather than point sampling.
 REX_STORE_U32(object+72,2);
 REX_STORE_U32(object+76,2);
}
