namespace {
thread_local bool stadiumLightmapPass = false;
thread_local bool stadiumGroundPass = false;
thread_local bool stadiumBackgroundPass = false;
thread_local bool stadiumBackgroundDepthTest = false;
thread_local bool stadiumLightmapAlpha = false;
constexpr uint64_t stadiumMaterialDirty = 0x0002000000000e00ull;
struct StadiumLightmapDrawState {
  uint32_t gpu = 0, depth = 0, color = 0, blend = 0, blendEnabled = 0;
  uint32_t viewportZScale=0,viewportZOffset=0;
  bool farBackground=false;
};
// 821167xx SetViewport writes these cache fields and marks bits8/4 at+24.
// Xenos registers: PA_CL_VPORT_ZSCALE and PA_CL_VPORT_ZOFFSET (0x2113/0x2114).
constexpr uint64_t stadiumViewportDepthDirty=0xCull;
thread_local StadiumLightmapDrawState stadiumLightmapDraw;
struct StadiumLightmapState {
  bool previousPass, previousAlpha, previousGround;
  StadiumLightmapState(uint8_t*, bool enabled, bool alpha = false, bool ground = false)
    : previousPass(stadiumLightmapPass), previousAlpha(stadiumLightmapAlpha), previousGround(stadiumGroundPass) {
    stadiumGroundPass = ground;
    stadiumLightmapPass = enabled;
    stadiumLightmapAlpha = alpha;
  }
  void Restore(PPCContext&, uint8_t*) {
    stadiumGroundPass = previousGround;
    stadiumLightmapPass = previousPass;
    stadiumLightmapAlpha = previousAlpha;
  }
};
}
extern "C" void pes6_stadium_lightmap_state(PPCContext& ctx, uint8_t* base) {
  const auto gpu = ctx.r3.u32;
  if (!gpu) return;
  if (!stadiumLightmapPass&&!stadiumBackgroundPass&&!stadiumGroundPass) {
    return;
  }
  // Capture the packet's actual material state after native material binding.
  // A model-level snapshot predates that binding and desynchronizes the cache.
  stadiumLightmapDraw = {gpu, REX_LOAD_U32(gpu + 10420), REX_LOAD_U32(gpu + 10428),
    REX_LOAD_U32(gpu + 10424), REX_LOAD_U32(gpu + 11756)};
  if(stadiumBackgroundPass){
    // The camera-relative dome is background, never a depth occluder for
    // the imported upper tiers beyond its 100-unit radius.
    REX_STORE_U32(gpu+10420,stadiumBackgroundDepthTest ?
      ((stadiumLightmapDraw.depth&~0x76u)|0x32u) : (stadiumLightmapDraw.depth&~0x76u));
    if(stadiumBackgroundDepthTest){
      // A dome can be submitted after foreground in entrance cameras.
      // LEQUAL at the dome's physical radius still overwrites farther stands.
      // z_window = z_ndc*0 + 1 makes it true background, independent of its
      // mesh radius, with depth testing retained and depth writes disabled.
      stadiumLightmapDraw.viewportZScale=REX_LOAD_U32(gpu+10392);
      stadiumLightmapDraw.viewportZOffset=REX_LOAD_U32(gpu+10396);
      stadiumLightmapDraw.farBackground=true;
      REX_STORE_U32(gpu+10392,0);
      REX_STORE_U32(gpu+10396,0x3f800000u);
      REX_STORE_U64(gpu+24,REX_LOAD_U64(gpu+24)|stadiumViewportDepthDirty);
    }
    REX_STORE_U64(gpu+16,REX_LOAD_U64(gpu+16)|stadiumMaterialDirty);
    return;
  }
  if(stadiumGroundPass){
    // Original coplanar apron: retain its opaque material and use LEQUAL.
    // Write its depth so the following original lightmap can use EQUAL.
    REX_STORE_U32(gpu+10420,(stadiumLightmapDraw.depth&~0x74u)|0x36u);
    REX_STORE_U64(gpu+16,REX_LOAD_U64(gpu+16)|stadiumMaterialDirty);
    return;
  }
  REX_STORE_U32(gpu + 10424, stadiumLightmapAlpha ? 0x01000706u : 0x01000008u);
  if (!stadiumLightmapDraw.blendEnabled) SunsetNative(ctx, base, sub_82115538, gpu, 1);
  REX_STORE_U32(gpu + 10420, (stadiumLightmapDraw.depth & ~0x74u) | 0x22u);
  REX_STORE_U32(gpu + 10428, stadiumLightmapDraw.color & ~0x18u);
  // Only depth/color/blend registers changed. Bit63 forces unrelated vertex
  // input rebuilding and must never be dirtied by a lightmap blend override.
  REX_STORE_U64(gpu + 16, REX_LOAD_U64(gpu + 16) | stadiumMaterialDirty);
}
extern "C" void pes6_stadium_lightmap_draw_end(PPCContext& ctx, uint8_t* base) {
  if (!stadiumLightmapDraw.gpu) return;
  const auto state = stadiumLightmapDraw;
  stadiumLightmapDraw = {};
  REX_STORE_U32(state.gpu + 10420, state.depth);
  REX_STORE_U32(state.gpu + 10428, state.color);
  REX_STORE_U32(state.gpu + 10424, state.blend);
  if(state.farBackground){
    REX_STORE_U32(state.gpu+10392,state.viewportZScale);
    REX_STORE_U32(state.gpu+10396,state.viewportZOffset);
    REX_STORE_U64(state.gpu+24,REX_LOAD_U64(state.gpu+24)|stadiumViewportDepthDirty);
  }
  if (!state.blendEnabled) SunsetNative(ctx, base, sub_82115538, state.gpu, state.blendEnabled);
  REX_STORE_U64(state.gpu + 16, REX_LOAD_U64(state.gpu + 16) | stadiumMaterialDirty);
}
