// Native presentation adapter. AMD EASU and RCAS are used from the licensed
// FidelityFX-FSR headers. Fusing them preserves the algorithms while avoiding
// an additional host render-target allocation in the binary renderer.
#define A_GPU 1
#define A_HLSL 1
#include "ffx_a.h"

struct PresentConstants {
    [[vk::offset(24)]] uint descriptor;
    uint descriptor2;
    float gamma;
    float displayCorrection;
};
[[vk::push_constant]] ConstantBuffer<PresentConstants> pc : register(b3,space4);
Texture2D<float4> images[] : register(t0,space0);
SamplerState samplers[] : register(s0,space3);
[[vk::constant_id(0)]] const uint OutputWidth=1920;
[[vk::constant_id(1)]] const uint OutputHeight=1080;
[[vk::constant_id(2)]] const uint FsrEnabled=1;
[[vk::constant_id(3)]] const uint FxaaQuality=0;
[[vk::constant_id(4)]] const float RcasReduction=0.0;

static AU4 easu0,easu1,easu2,easu3;
static AF2 centerUv,inverseInput;
static ASU2 centerPixel;
AF4 FsrEasuRF(AF2 uv){return max(images[pc.descriptor].GatherRed(samplers[0],uv),0.0);}
AF4 FsrEasuGF(AF2 uv){return max(images[pc.descriptor].GatherGreen(samplers[0],uv),0.0);}
AF4 FsrEasuBF(AF2 uv){return max(images[pc.descriptor].GatherBlue(samplers[0],uv),0.0);}
AF3 EasuPixel(ASU2 p);
AF4 FsrRcasLoadF(ASU2 p){return AF4(EasuPixel(p),1.0);}
void FsrRcasInputF(inout AF1 r,inout AF1 g,inout AF1 b){}
#define FSR_EASU_F 1
#define FSR_RCAS_F 1
#include "ffx_fsr1.h"

AF3 EasuPixel(ASU2 p){
    p=clamp(p,ASU2(0,0),ASU2(OutputWidth-1,OutputHeight-1));
    AF3 color;
    if(FsrEnabled!=0)FsrEasuF(color,AU2(p),easu0,easu1,easu2,easu3);
    else color=images[pc.descriptor].SampleLevel(samplers[0],centerUv+AF2(p-centerPixel)*inverseInput,0).rgb;
    return color;
}
float3 Fxaa(float2 uv,float2 invSize){
    float3 nw=images[pc.descriptor].SampleLevel(samplers[0],uv+float2(-1,-1)*invSize,0).rgb;
    float3 ne=images[pc.descriptor].SampleLevel(samplers[0],uv+float2( 1,-1)*invSize,0).rgb;
    float3 sw=images[pc.descriptor].SampleLevel(samplers[0],uv+float2(-1, 1)*invSize,0).rgb;
    float3 se=images[pc.descriptor].SampleLevel(samplers[0],uv+float2( 1, 1)*invSize,0).rgb;
    float3 mid=images[pc.descriptor].SampleLevel(samplers[0],uv,0).rgb;
    const float3 weights=float3(0.299,0.587,0.114);
    float lNW=dot(nw,weights),lNE=dot(ne,weights),lSW=dot(sw,weights),lSE=dot(se,weights),lM=dot(mid,weights);
    float low=min(lM,min(min(lNW,lNE),min(lSW,lSE))),high=max(lM,max(max(lNW,lNE),max(lSW,lSE)));
    float2 dir=float2(-((lNW+lNE)-(lSW+lSE)),(lNW+lSW)-(lNE+lSE));
    float reduce=max((lNW+lNE+lSW+lSE)*(FxaaQuality>1?0.015625:0.03125),0.0078125);
    dir=clamp(dir/(min(abs(dir.x),abs(dir.y))+reduce),-(FxaaQuality>1?16.0:8.0),FxaaQuality>1?16.0:8.0)*invSize;
    float3 a=0.5*(images[pc.descriptor].SampleLevel(samplers[0],uv+dir*(1.0/3.0-0.5),0).rgb+images[pc.descriptor].SampleLevel(samplers[0],uv+dir*(2.0/3.0-0.5),0).rgb);
    float3 b=a*0.5+0.25*(images[pc.descriptor].SampleLevel(samplers[0],uv-dir*0.5,0).rgb+images[pc.descriptor].SampleLevel(samplers[0],uv+dir*0.5,0).rgb);
    float lB=dot(b,weights);return lB<low||lB>high?a:b;
}
float4 main(float4 position:SV_Position,float2 uv:TEXCOORD):SV_Target{
    uint width,height;images[pc.descriptor].GetDimensions(width,height);
    centerUv=uv;inverseInput=1.0/AF2(width,height);
    FsrEasuCon(easu0,easu1,easu2,easu3,float(width),float(height),float(width),float(height),float(OutputWidth),float(OutputHeight));
    float3 color;
    if(FsrEnabled==0&&FxaaQuality!=0)color=Fxaa(uv,1.0/float2(width,height));
    else {
        AU4 sharpness;FsrRcasCon(sharpness,RcasReduction);
        float2 outputPosition=clamp(uv*float2(OutputWidth,OutputHeight)-0.5,0.0,float2(OutputWidth-1,OutputHeight-1));
        AU2 outputPixel=AU2(outputPosition);centerPixel=ASU2(outputPixel);
        // Preserve continuous UVs when a window is smaller than the selected
        // output size, rather than snapping the image to virtual output pixels.
        easu0.zw=asuint(asfloat(easu0.zw)+frac(outputPosition)*asfloat(easu0.xy));
        FsrRcasF(color.r,color.g,color.b,outputPixel,sharpness);
    }
    float3 ramp=pow(max(color,0.0),pc.gamma);
    float3 linearLight=lerp(ramp/12.92,pow((ramp+0.055)/1.055,2.4),step(0.04045,ramp));
    float3 corrected=lerp(linearLight*4.5,1.099*pow(linearLight,0.45)-0.099,step(0.018,linearLight));
    return float4(lerp(ramp,corrected,pc.displayCorrection),1.0);
}
