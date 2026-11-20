#pragma once
// Rest-space color is immutable between dirt events. Pose skinning stays live.
struct BakedDirtVertex {Vec bary{};uint32_t color=0;};
struct BakedDirtPiece {unsigned sample=0;std::array<unsigned,3> corners{};std::vector<BakedDirtVertex> vertices;std::vector<std::array<unsigned,3>> triangles;};
struct BakedDirtSkin {PlayerSurfaceVertex vertex;unsigned sample=0;};
struct DirtProbe {Vec bary{},p{},n{};uint32_t wet_rgb=0,dry_rgb=0;float grain_a=0,grain_b=0;};
struct DirtTopologyPiece {unsigned sample=0;std::vector<DirtProbe> vertices;std::vector<std::array<unsigned,3>> triangles;};
struct DirtTopology {uint64_t model=0,visibility=0,used=0;std::vector<DirtTopologyPiece> pieces;};
std::array<DirtTopology,128> dirt_topology_cache;
uint64_t dirt_topology_clock=0;
struct BakedDirt {uint64_t model=0,signature=0,visibility=0,used=0;unsigned climate=0;
  std::shared_ptr<const std::vector<DirtPatch>> marks;unsigned count=0;std::vector<BakedDirtPiece> pieces;std::vector<BakedDirtSkin> skin;};
std::array<BakedDirt,128> baked_dirt_cache;
uint64_t dirt_bake_clock=0;
Vec DirtPoint(const PlayerSurfaceSample& s,Vec b){return Add(Add(Mul(s.triangle[0].p,b.x),Mul(s.triangle[1].p,b.y)),Mul(s.triangle[2].p,b.z));}
Vec DirtNormal(const PlayerSurfaceSample& s,Vec b){return Unit(Add(Add(Mul(s.triangle[0].n,b.x),Mul(s.triangle[1].n,b.y)),Mul(s.triangle[2].n,b.z)));}
Vec DirtProject(Vec p,unsigned axis){return axis==0?Vec{p.y,p.z,0}:axis==1?Vec{p.x,p.z,0}:Vec{p.x,p.y,0};}
float DirtCross2(Vec a,Vec b){return a.x*b.y-a.y*b.x;}
unsigned DirtAxis(Vec n){return std::abs(n.x)>std::abs(n.y)&&std::abs(n.x)>std::abs(n.z)?0:std::abs(n.y)>std::abs(n.z)?1:2;}
// Subtract the complete transparent print plane from the underlying dirt pass.
// The print plane then receives the same dirt ONCE, including its background.
std::vector<std::vector<Vec>> DirtSubtractPrint(const PlayerSurfaceSample& source,const std::vector<Vec>& polygon,
                                               const PlayerSurfaceSample& print,unsigned axis){
  std::vector<std::vector<Vec>> outside;
  std::vector<Vec> inside=polygon;
  const auto a=DirtProject(print.triangle[0].p,axis),b=DirtProject(print.triangle[1].p,axis),c=DirtProject(print.triangle[2].p,axis);
  const float area=DirtCross2(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));if(std::abs(area)<1e-9f)return {polygon};
  const std::array<Vec,3> points{a,b,c};const float sign=area>0?1.f:-1.f;
  for(unsigned edge=0;edge<3&&!inside.empty();edge++){
    const auto origin=points[edge],direction=Add(points[(edge+1)%3],Mul(origin,-1));
    const auto side=[&](Vec bary){return sign*DirtCross2(direction,Add(DirtProject(DirtPoint(source,bary),axis),Mul(origin,-1)));};
    std::vector<Vec> next,part;
    for(unsigned i=0;i<inside.size();i++){
      const auto p=inside[i],q=inside[(i+1)%inside.size()];const float dp=side(p),dq=side(q);
      if(dp>=0)next.push_back(p);else part.push_back(p);
      if((dp>=0)!=(dq>=0)){const float t=dp/(dp-dq);const auto cut=Add(Mul(p,1-t),Mul(q,t));next.push_back(cut);part.push_back(cut);}
    }
    if(part.size()>=3)outside.push_back(std::move(part));inside=std::move(next);
  }
  return outside;
}
bool DirtProjectedBary(const PlayerSurfaceSample& s,Vec p,unsigned axis,Vec& bary){
  const auto a=DirtProject(s.triangle[0].p,axis),b=DirtProject(s.triangle[1].p,axis),c=DirtProject(s.triangle[2].p,axis),q=DirtProject(p,axis);
  if(q.x<std::min({a.x,b.x,c.x})-.00001f||q.x>std::max({a.x,b.x,c.x})+.00001f||
     q.y<std::min({a.y,b.y,c.y})-.00001f||q.y>std::max({a.y,b.y,c.y})+.00001f)return false;
  const float denominator=DirtCross2(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));if(std::abs(denominator)<1e-9f)return false;
  bary.y=DirtCross2(Add(q,Mul(a,-1)),Add(c,Mul(a,-1)))/denominator;
  bary.z=DirtCross2(Add(b,Mul(a,-1)),Add(q,Mul(a,-1)))/denominator;bary.x=1-bary.y-bary.z;
  return std::min({bary.x,bary.y,bary.z})>=-.0001f;
}
uint64_t DirtSignature(const PlayerDirt& dirt){
  uint64_t hash=1469598103934665603ull;
  for(unsigned i=0;i<dirt.patch_count;i++){const auto& p=dirt.patches[i];
    for(uint32_t v:{p.region,p.event,p.birth_tick,std::bit_cast<uint32_t>(p.strength),std::bit_cast<uint32_t>(p.center.x),
      std::bit_cast<uint32_t>(p.center.y),std::bit_cast<uint32_t>(p.center.z),std::bit_cast<uint32_t>(p.normal.x),
      std::bit_cast<uint32_t>(p.normal.y),std::bit_cast<uint32_t>(p.normal.z),uint32_t(p.has_uv),
      std::bit_cast<uint32_t>(p.uv.x),std::bit_cast<uint32_t>(p.uv.y),uint32_t(p.source_model),uint32_t(p.source_model>>32)})hash=(hash^v)*1099511628211ull;
  }return hash;
}
// A permanent mark belongs to the fabric, not to one LOD's rest-space shape.
// Reproject its UV anchor only on a bake miss; never change live/replay marks.
DirtPatch DirtPatchForModel(const DirtPatch& original,const PlayerSurfaceModel& model,const DirtTopology& topology){
  if(!original.has_uv||original.source_model==model.revision)return original;
  DirtPatch result=original;float best=1e9f;
  for(const auto& piece:topology.pieces){const auto& s=model.samples[piece.sample];
    if(s.print_overlay||s.region!=original.region)continue;
    const auto& t=s.triangle;if(!t[0].has_uv||!t[1].has_uv||!t[2].has_uv)continue;
    const auto a=t[0].uv,b=t[1].uv,c=t[2].uv,q=original.uv;
    if(q.x<std::min({a.x,b.x,c.x})-.0001f||q.x>std::max({a.x,b.x,c.x})+.0001f||
       q.y<std::min({a.y,b.y,c.y})-.0001f||q.y>std::max({a.y,b.y,c.y})+.0001f)continue;
    const float area=DirtCross2(Add(b,Mul(a,-1)),Add(c,Mul(a,-1)));if(std::abs(area)<1e-12f)continue;
    Vec bary{};bary.y=DirtCross2(Add(q,Mul(a,-1)),Add(c,Mul(a,-1)))/area;
    bary.z=DirtCross2(Add(b,Mul(a,-1)),Add(q,Mul(a,-1)))/area;bary.x=1-bary.y-bary.z;
    if(std::min({bary.x,bary.y,bary.z})<-.001f)continue;
    const auto p=DirtPoint(s,bary),n=DirtNormal(s,bary);
    const float facing=n.x*original.normal.x+n.y*original.normal.y+n.z*original.normal.z;
    if(facing<.25f)continue; // mirrored atlas islands must not move dirt across the body.
    const auto delta=Add(p,Mul(original.center,-1));
    const float score=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z+.01f*(1-facing);
    if(score<best){best=score;result.center=p;result.normal=n;}
  }
  return result; // Missing/degenerate UVs retain the original physical anchor.
}
template<class Visible> const DirtTopology& GetDirtTopology(const PlayerSurfaceModel& model,uint64_t visibility,Visible visible){
  const auto clock=++dirt_topology_clock;
  for(auto& t:dirt_topology_cache)if(t.model==model.revision&&t.visibility==visibility){t.used=clock;return t;}
  auto* topology=&dirt_topology_cache[0];for(auto& t:dirt_topology_cache)if(t.used<topology->used)topology=&t;
  *topology={};topology->model=model.revision;topology->visibility=visibility;topology->used=clock;
  std::vector<unsigned> active,prints,cloth;
  for(unsigned i=0;i<model.samples.size();i++)if(visible(model.samples[i])){
    active.push_back(i);(model.samples[i].print_overlay?prints:cloth).push_back(i);
  }
  for(const auto index:active){
    const auto& s=model.samples[index];const auto a=s.triangle[0].p,b=s.triangle[1].p,c=s.triangle[2].p;
    const auto normal=DirtNormal(s,{1.f/3,1.f/3,1.f/3}),center=DirtPoint(s,{1.f/3,1.f/3,1.f/3});
    const auto axis=DirtAxis(normal);
    std::vector<std::vector<Vec>> polygons{{{1,0,0},{0,1,0},{0,0,1}}};
    for(const auto other:prints){
      if(other==index||(s.print_overlay&&other>index))continue;
      const auto& print=model.samples[other];const auto pn=DirtNormal(print,{1.f/3,1.f/3,1.f/3});
      if(normal.x*pn.x+normal.y*pn.y+normal.z*pn.z<.65f)continue;
      const auto delta=Add(DirtPoint(print,{1.f/3,1.f/3,1.f/3}),Mul(center,-1));
      const float depth=delta.x*normal.x+delta.y*normal.y+delta.z*normal.z;if(depth<-.003f||depth>.035f)continue;
      const auto p0=DirtProject(print.triangle[0].p,axis),p1=DirtProject(print.triangle[1].p,axis),p2=DirtProject(print.triangle[2].p,axis);
      const auto q0=DirtProject(a,axis),q1=DirtProject(b,axis),q2=DirtProject(c,axis);
      if(std::max({p0.x,p1.x,p2.x})<std::min({q0.x,q1.x,q2.x})||std::min({p0.x,p1.x,p2.x})>std::max({q0.x,q1.x,q2.x})||
         std::max({p0.y,p1.y,p2.y})<std::min({q0.y,q1.y,q2.y})||std::min({p0.y,p1.y,p2.y})>std::max({q0.y,q1.y,q2.y}))continue;
      std::vector<std::vector<Vec>> next;
      for(const auto& polygon:polygons){auto parts=DirtSubtractPrint(s,polygon,print,axis);for(auto& part:parts)next.push_back(std::move(part));}
      polygons=std::move(next);if(polygons.empty())break;
    }
    DirtTopologyPiece piece{};piece.sample=index;
    // A printed triangle only projects onto nearby cloth. Build this small
    // candidate set once, not a full garment scan for every subdivision vertex.
    std::vector<unsigned> underneath;
    if(s.print_overlay){const auto p0=DirtProject(a,axis),p1=DirtProject(b,axis),p2=DirtProject(c,axis);
      for(const auto source:cloth){const auto& under=model.samples[source];
        if(!GarmentCompatible(s.region,under.region))continue;
        const auto q0=DirtProject(under.triangle[0].p,axis),q1=DirtProject(under.triangle[1].p,axis),q2=DirtProject(under.triangle[2].p,axis);
        if(std::max({p0.x,p1.x,p2.x})<std::min({q0.x,q1.x,q2.x})-.00001f||std::min({p0.x,p1.x,p2.x})>std::max({q0.x,q1.x,q2.x})+.00001f||
           std::max({p0.y,p1.y,p2.y})<std::min({q0.y,q1.y,q2.y})-.00001f||std::min({p0.y,p1.y,p2.y})>std::max({q0.y,q1.y,q2.y})+.00001f)continue;
        underneath.push_back(source);
      }
    }
    const auto vertex=[&](Vec bary){
      Vec p=DirtPoint(s,bary),n=DirtNormal(s,bary);
      if(s.print_overlay){float best=.035f;const auto original_p=p,original_n=n;
        for(const auto source:underneath){const auto& under=model.samples[source];Vec b;
          if(!GarmentCompatible(s.region,under.region)||!DirtProjectedBary(under,original_p,axis,b))continue;
          const auto q=DirtPoint(under,b),qn=DirtNormal(under,b);
          if(original_n.x*qn.x+original_n.y*qn.y+original_n.z*qn.z<.65f)continue;
          const auto delta=Add(original_p,Mul(q,-1));const float gap=delta.x*original_n.x+delta.y*original_n.y+delta.z*original_n.z;
          if(gap>=-.003f&&std::abs(gap)<best){best=std::abs(gap);p=q;n=qn;}
        }
      }
      const auto rgb=[&](bool wet){const float grass=wet?std::clamp(.14f+.08f*std::sin(p.x*61+p.y*39+p.z*47),.06f,.22f):std::clamp(.48f+.22f*std::sin(p.x*61+p.y*39+p.z*47),.26f,.70f);
        const auto blend=[&](float mud,float green){return uint32_t(mud*(1-grass)+green*grass);};
        return (blend(wet?59:76,57)<<16)|(blend(wet?43:58,79)<<8)|blend(wet?29:37,30);};
      piece.vertices.push_back({bary,p,n,rgb(true),rgb(false),std::sin(p.x*137+p.y*83+p.z*61),std::sin(p.y*293-p.x*181)});return unsigned(piece.vertices.size()-1);
    };
    for(const auto& polygon:polygons)for(unsigned fan=1;fan+1<polygon.size();fan++){
      const std::array<Vec,3> corners{polygon[0],polygon[fan],polygon[fan+1]};float longest=0;
      for(unsigned edge=0;edge<3;edge++){const auto d=Add(DirtPoint(s,corners[edge]),Mul(DirtPoint(s,corners[(edge+1)%3]),-1));longest=std::max(longest,std::hypot(d.x,d.y,d.z));}
      // Fixed physical spacing, independent of camera/LOD and mark count.
      // Small dry marks must still have samples on the coarser native LODs.
      const unsigned divisions=std::clamp(unsigned(std::ceil(longest/.035f)),2u,12u);
      std::array<std::array<unsigned,13>,13> grid;
      for(unsigned row=0;row<=divisions;row++)for(unsigned col=0;col<=divisions-row;col++)
        grid[row][col]=vertex(Add(Add(Mul(corners[0],1-float(row+col)/divisions),Mul(corners[1],float(row)/divisions)),Mul(corners[2],float(col)/divisions)));
      const auto triangle=[&](unsigned a,unsigned b,unsigned c){
        const auto p=piece.vertices[a].bary,q=piece.vertices[b].bary,r=piece.vertices[c].bary;
        const auto cross=Cross(Add(q,Mul(p,-1)),Add(r,Mul(p,-1)));
        if(std::hypot(cross.x,cross.y,cross.z)>1e-8f)piece.triangles.push_back({a,b,c});};
      for(unsigned row=0;row<divisions;row++)for(unsigned col=0;col<divisions-row;col++){
        triangle(grid[row][col],grid[row+1][col],grid[row][col+1]);
        if(row+col+1<divisions)triangle(grid[row+1][col],grid[row+1][col+1],grid[row][col+1]);
      }
    }
    if(!piece.triangles.empty())topology->pieces.push_back(std::move(piece));
  }
  return *topology;
}
const BakedDirt& BakePlayerDirt(const PlayerSurfaceModel& model,const PlayerDirt& dirt,unsigned climate,const DirtTopology& topology){
  const auto clock=++dirt_bake_clock;
  // Retaining marks makes their COW identity immutable: unchanged live/replay
  // snapshots need no O(mark count) hashing on every player draw.
  for(auto& b:baked_dirt_cache)if(b.model==model.revision&&b.marks==dirt.patches.data&&b.visibility==topology.visibility&&b.climate==climate&&b.count==dirt.patch_count){
    b.used=clock;++dirt_bake_hits;return b;
  }
  const auto signature=DirtSignature(dirt);
  for(auto& b:baked_dirt_cache)if(b.model==model.revision&&b.signature==signature&&b.visibility==topology.visibility&&b.climate==climate&&b.count==dirt.patch_count){
    b.used=clock;b.marks=dirt.patches.data;++dirt_bake_hits;return b;
  }
  const auto start=std::chrono::steady_clock::now();++dirt_bake_misses;
  auto* baked=&baked_dirt_cache[0];for(auto& b:baked_dirt_cache)if(b.used<baked->used)baked=&b;
  *baked={};baked->model=model.revision;baked->signature=signature;baked->visibility=topology.visibility;baked->climate=climate;
  baked->used=clock;baked->marks=dirt.patches.data;baked->count=dirt.patch_count;
  std::vector<DirtPatch> projected;projected.reserve(dirt.patch_count);
  for(unsigned i=0;i<dirt.patch_count;i++)projected.push_back(DirtPatchForModel(dirt.patches[i],model,topology));
  std::array<std::vector<const DirtPatch*>,dirt_region_count> eligible;
  for(unsigned i=0;i<dirt.patch_count;i++)for(unsigned region=0;region<dirt_region_count;region++)
    if(GarmentCompatible(region,projected[i].region))eligible[region].push_back(&projected[i]);
  // One scratch allocation per bake instead of allocating a mark list for
  // every garment triangle; preserve candidate order and all field math.
  std::vector<const DirtPatch*> marks;marks.reserve(dirt.patch_count);
  for(const auto& source:topology.pieces){
    const auto& s=model.samples[source.sample];const auto a=s.triangle[0].p,b=s.triangle[1].p,c=s.triangle[2].p;
    marks.clear();
    const auto distance=[](float at,float a,float b,float c,float radius){return std::max({std::min({a,b,c})-at,at-std::max({a,b,c}),0.f})/radius;};
    for(const auto* p:eligible[s.region]){
      const auto r=DirtRadii(climate,p->region);
      const float x=distance(p->center.x,a.x,b.x,c.x,r.x),y=distance(p->center.y,a.y,b.y,c.y,r.y),z=distance(p->center.z,a.z,b.z,c.z,r.z);
      if(x*x+y*y+z*z<(s.print_overlay?1.7f:1.35f))marks.push_back(p);
    }if(marks.empty())continue;
    BakedDirtPiece piece{};piece.sample=source.sample;piece.vertices.reserve(source.vertices.size());
    for(const auto& v:source.vertices){float alpha=0;
      for(const auto* patch:marks)alpha=1-(1-alpha)*(1-DirtFieldPrepared(*patch,v.p,v.n,s.region,climate,v.grain_a,v.grain_b));
      piece.vertices.push_back({v.bary,(uint32_t(std::clamp(alpha*255.f,0.f,230.f))<<24)|(climate?v.wet_rgb:v.dry_rgb)});
    }
    for(const auto& triangle:source.triangles)if((piece.vertices[triangle[0]].color|piece.vertices[triangle[1]].color|piece.vertices[triangle[2]].color)>>24)
      piece.triangles.push_back(triangle);
    if(!piece.triangles.empty()){
      std::vector<unsigned> remap(piece.vertices.size(),~0u);std::vector<BakedDirtVertex> compact;
      for(auto& triangle:piece.triangles)for(auto& index:triangle){
        auto& mapped=remap[index];if(mapped==~0u){mapped=unsigned(compact.size());compact.push_back(piece.vertices[index]);}index=mapped;
      }
      piece.vertices=std::move(compact);baked->pieces.push_back(std::move(piece));
    }
  }
  // Shared native corners are skinned once, irrespective of the number of
  // adjacent triangles or stain subdivisions. Keep the hem palette distinct.
  std::unordered_map<std::string,unsigned> shared;
  for(auto& piece:baked->pieces){const auto& s=model.samples[piece.sample];
    for(unsigned j=0;j<3;j++){
      auto v=s.triangle[j];v.p=Add(v.p,Mul(v.n,.0045f));
      std::array<uint32_t,12> key{std::bit_cast<uint32_t>(v.p.x),std::bit_cast<uint32_t>(v.p.y),std::bit_cast<uint32_t>(v.p.z),
        v.bone[0],v.bone[1],v.bone[2],v.bone[3],std::bit_cast<uint32_t>(v.weight[0]),std::bit_cast<uint32_t>(v.weight[1]),
        std::bit_cast<uint32_t>(v.weight[2]),std::bit_cast<uint32_t>(v.weight[3]),uint32_t(s.cloth_hem)};
      const auto [it,inserted]=shared.emplace(std::string(reinterpret_cast<const char*>(key.data()),sizeof(key)),unsigned(baked->skin.size()));
      if(inserted)baked->skin.push_back({v,piece.sample});piece.corners[j]=it->second;
    }
  }
  dirt_bake_us+=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count());
  return *baked;
}
