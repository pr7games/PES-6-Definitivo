#include "stadium_net_upload.h"
#include <cmath>

namespace {
uint32_t stadiumAdvertTick = 0;
uint32_t stadiumAdvertReplayTick = 0;
int64_t stadiumAdvertReplayOrigin = 0;
bool stadiumAdvertInReplay = false;

struct StadiumAdvertStream {
  pes6::stadium_net::PositionStream stream;
  // Atlas row and within-row coordinate are invariant across replay seeks.
  std::vector<std::array<float,2>> uvRows;
};

float StadiumAdvertPhase(uint8_t* base) {
  const auto tick = (base[0x8313A93Eu] ? stadiumAdvertReplayTick : stadiumAdvertTick) % 4320;
  const auto within = tick % 1080;
  float phase = float(tick / 1080) * 0.25f;
  if (within >= 1020) phase += float(within - 1020) * (1.0f / 60.0f) * 0.25f;
  return phase;
}

std::vector<StadiumAdvertStream> StadiumAdvertStreams(uint8_t* base, uint32_t data) {
  std::vector<StadiumAdvertStream> streams;
  const auto count = REX_LOAD_U32(data + 40);
  const auto packets = data + REX_LOAD_U32(data + 44);
  const auto types = data + REX_LOAD_U32(data + 84);
  const auto names = data + REX_LOAD_U32(data + 128);
  for (uint32_t i = 0; i < count; ++i) {
    const auto packet = packets + i * 96;
    if (!base[packet + 44]) continue;
    const auto sampler = types + REX_LOAD_U16(packet + 48) * 80;
    const auto texture = REX_LOAD_U32(names + REX_LOAD_U16(sampler + 14) * 16 + 12);
    if (texture != 25014) continue;
    const auto descriptor = packet + REX_LOAD_U32(packet + 20);
    const auto vertices = REX_LOAD_U32(descriptor + 4);
    if (base[descriptor + 9] != 32 || vertices > 57000) continue;
    const auto source = descriptor + REX_LOAD_U32(descriptor);
    StadiumAdvertStream stream{{descriptor, source, vertices * 32}, {}};
    stream.uvRows.reserve(vertices);
    for (uint32_t vertex = 0; vertex < vertices; ++vertex) {
      const float v=std::bit_cast<float>(REX_LOAD_U32(source + vertex * 32 + 28));
      const float row=std::floor(v*4.0f)*0.25f;
      stream.uvRows.push_back({row,v-row});
    }
    streams.push_back(std::move(stream));
  }
  return streams;
}

bool StadiumAdvertUpload(PPCContext& original, uint8_t* base, uint32_t model,
                         const std::vector<StadiumAdvertStream>& streams,
                         const std::vector<pes6::stadium_net::PositionStream>& descriptors,float phase) {
  if (streams.empty()) return true;
  const auto uploads = pes6::stadium_net::ResolveUploads(model, descriptors,
    [&](uint32_t at) { return REX_LOAD_U32(at); },
    [&](uint32_t at) { return base[at]; });
  if (uploads.empty()) return false;
  for (const auto& stream : streams) {
    for (size_t vertex = 0; vertex < stream.uvRows.size(); ++vertex) {
      const float row = stream.uvRows[vertex][0];
      const float offset = row + phase >= 1.0f ? 0.0f : row + phase;
      REX_STORE_U32(stream.stream.source + uint32_t(vertex) * 32 + 28, std::bit_cast<uint32_t>(stream.uvRows[vertex][1] + offset));
    }
  }
  for (const auto& upload : uploads) {
    const auto vtable = REX_LOAD_U32(upload.resource);
    const auto function = REX_LOAD_U32(vtable + 24);
    PPCContext ctx = original;
    ctx.r3.u64 = upload.resource;
    ctx.r4.u64 = upload.source;
    ctx.r5.u64 = upload.bytes;
    REX_CALL_INDIRECT_FUNC(function);
  }
  return true;
}
}

extern "C" void pes6_stadium_advert_tick(uint8_t* base) {
  const auto config = REX_LOAD_U32(0x83139360u);
  if (!config || (base[config + 130] != 8 && base[config + 130] != 10) || !base[config + 129] || base[0x8313A93Eu]) return;
  stadiumAdvertTick = (stadiumAdvertTick + 1) % 4320;
  stadiumAdvertInReplay = false;
}

extern "C" void pes6_stadium_advert_replay(uint8_t* base, uint32_t manager) {
  const auto config = REX_LOAD_U32(0x83139360u);
  if (!config || (base[config + 130] != 8 && base[config + 130] != 10) || !manager) return;
  const float cursor = std::bit_cast<float>(REX_LOAD_U32(manager + 36));
  if (!std::isfinite(cursor) || cursor < 0.0f || cursor > 10000000.0f) return;
  const auto tick = uint32_t(cursor);
  if (!stadiumAdvertInReplay) {
    stadiumAdvertReplayOrigin = int64_t(stadiumAdvertTick) - tick;
    stadiumAdvertInReplay = true;
  }
  const auto position = (stadiumAdvertReplayOrigin + tick) % 4320;
  stadiumAdvertReplayTick = uint32_t(position < 0 ? position + 4320 : position);
}

