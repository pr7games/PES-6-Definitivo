#pragma once
#include "cloth_contact.h"
namespace pes6::cloth {
// Bind the independent lettering surface to triangles of the actual outside
// shirt. Only draw-local palette translations change; shared meshes stay native.
struct PrintAnchor{std::array<ContactVertex,3> tri;std::array<float,3> bary;float orientation=1;};
struct PrintClearanceProbe{std::array<unsigned,3> indices;std::array<float,3> bary;PrintAnchor anchor;};
struct PrintAttachment{
 uint32_t data=0,packet=0;std::vector<ContactVertex> vertices;std::vector<PrintAnchor> anchors;
 std::vector<unsigned> bones;std::vector<double> factor;
 std::vector<std::vector<double>> rows;Vec center{};
 std::vector<Vec> targets;
 std::vector<PrintClearanceProbe> clearance;
 std::array<Frame,50> output{};Frame render_root{},inverse_root{};bool prepared=false,ready=false;float error=0;
 void Factor(){
  for(const auto& v:vertices)for(auto inf:v.influence)if(inf.weight>0&&std::find(bones.begin(),bones.end(),inf.bone)==bones.end())bones.push_back(inf.bone);
  for(const auto& v:vertices)center=center+v.p*(1.f/vertices.size());
  const auto n=bones.size()*4;factor.assign(n*n,0);
  for(const auto& v:vertices){std::vector<double> row(n);const auto local=(v.p-center)*10.f;const double basis[4]={1,local.x,local.y,local.z};for(auto inf:v.influence)if(inf.weight>0){const auto i=std::find(bones.begin(),bones.end(),inf.bone)-bones.begin();for(unsigned k=0;k<4;k++)row[i*4+k]+=inf.weight*basis[k];}for(size_t i=0;i<n;i++)for(size_t j=0;j<n;j++)factor[i*n+j]+=row[i]*row[j];rows.push_back(std::move(row));}
  for(size_t i=0;i<n;i++)factor[i*n+i]+=1e-7;
  for(size_t i=0;i<n;i++)for(size_t j=0;j<=i;j++){double x=factor[i*n+j];for(size_t k=0;k<j;k++)x-=factor[i*n+k]*factor[j*n+k];factor[i*n+j]=i==j?std::sqrt(std::max(x,1e-12)):x/factor[j*n+j];}
 }
 Vec Target(size_t i,const std::array<Frame,50>& frames)const{
  const auto& a=anchors[i];std::array<Vec,3> p;for(unsigned k=0;k<3;k++)p[k]=SkinContact(a.tri[k],frames);
  const auto normal=Unit(Cross(p[1]-p[0],p[2]-p[0]))*a.orientation;
  // Sparse native digit triangles can cross a shirt fold between anchors.
  // Keep a little more draw clearance while retaining the surface binding.
  return p[0]*a.bary[0]+p[1]*a.bary[1]+p[2]*a.bary[2]+normal*.005f;
 }
 void Solve(const std::array<Frame,50>& frames){
  const auto n=bones.size()*4;std::vector<std::array<double,3>> rhs(n);targets.resize(vertices.size());
  for(size_t i=0;i<vertices.size();i++){targets[i]=Target(i,frames);const auto delta=targets[i]-SkinContact(vertices[i],frames);for(size_t k=0;k<n;k++){rhs[k][0]+=delta.x*rows[i][k];rhs[k][1]+=delta.y*rows[i][k];rhs[k][2]+=delta.z*rows[i][k];}}
  for(unsigned axis=0;axis<3;axis++){for(size_t i=0;i<n;i++){for(size_t j=0;j<i;j++)rhs[i][axis]-=factor[i*n+j]*rhs[j][axis];rhs[i][axis]/=factor[i*n+i];}for(size_t i=n;i-->0;){for(size_t j=i+1;j<n;j++)rhs[i][axis]-=factor[j*n+i]*rhs[j][axis];rhs[i][axis]/=factor[i*n+i];}}
  output=frames;for(size_t i=0;i<bones.size();i++){
   const auto coefficient=[&](unsigned k){return Vec{float(rhs[i*4+k][0]),float(rhs[i*4+k][1]),float(rhs[i*4+k][2])};};
   const auto dx=coefficient(1)*10.f,dy=coefficient(2)*10.f,dz=coefficient(3)*10.f;auto& frame=output[bones[i]];
   frame.x=frame.x+dx;frame.y=frame.y+dy;frame.z=frame.z+dz;frame.p=frame.p+coefficient(0)-dx*center.x-dy*center.y-dz*center.z;
  }
  // The rasterized digit is planar BETWEEN vertices. Check those interiors
  // against the deformed shirt too, and translate its draw palette together.
  Vec shift{};
  for(unsigned pass=0;pass<2;pass++)for(const auto& probe:clearance){
   std::array<Vec,3> cloth;for(unsigned k=0;k<3;k++)cloth[k]=SkinContact(probe.anchor.tri[k],frames);
   const auto normal=Unit(Cross(cloth[1]-cloth[0],cloth[2]-cloth[0]))*probe.anchor.orientation;
   const auto surface=cloth[0]*probe.anchor.bary[0]+cloth[1]*probe.anchor.bary[1]+cloth[2]*probe.anchor.bary[2];
   Vec print=shift;for(unsigned k=0;k<3;k++)print=print+SkinContact(vertices[probe.indices[k]],output)*probe.bary[k];
   const float gap=Dot(print-surface,normal);if(gap<.002f)shift=Limit(shift+normal*(.002f-gap),.025f);
  }
  for(unsigned b:bones)output[b].p=output[b].p+shift;
  error=0;for(size_t i=0;i<vertices.size();i++)error=std::max(error,Length(SkinContact(vertices[i],output)-targets[i]));
 }
};
}
