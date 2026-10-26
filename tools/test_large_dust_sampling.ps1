$ErrorActionPreference = 'Stop'
# Numerical regression of the shader's float-sized geometric-cell schedule.
# Does not claim to validate the final rendered image or frame time.
Add-Type -TypeDefinition @'
using System;
public static class LargeDustSamplingTest {
 public static void Run() {
  int cases = 0, maximumSteps = 0;
  foreach(float start in new float[] {0, 1, 447, 10000, 250000, 5000000, 1e8f})
  foreach(float span in new float[] {1, 5500, 22000, 500000, 5000000, 1e8f, 1e12f}) {
   float end = start + span;
   if(end <= start) continue; // Below single-precision positional resolution.
   const float cadence = 446.875f;
   float origin = cadence * 32;
   float index = (float)Math.Floor(Math.Log(1 + start/origin) / Math.Log(1.03125));
   float lattice = origin * ((float)Math.Pow(1.03125,index)-1);
   float cursor = start;
   double integrated = 0;
   int steps = 0;
   for(;steps < 1024 && cursor < end; ++steps) {
    float length = Math.Max(cadence,(lattice+origin)*0.03125f);
    float next = Math.Min(end,lattice+length);
    float cell = Math.Max(0,next-cursor);
    if(cell > 0) { integrated += cell; cursor = next; }
    lattice += length;
   }
   if(cursor < end || Math.Abs(integrated-(end-start)) > Math.Max(0.1,(end-start)*1e-5))
    throw new Exception("Incomplete interval: " + start + ", " + span);
   maximumSteps = Math.Max(maximumSteps,steps); ++cases;
  }
  // Centered ellipsoid rays must remain valid regardless of axis scaling.
  foreach(float extent in new float[] {500000, 5000000, 5e7f, 5e8f, 5e9f})
  for(int i=0;i<=90;++i) {
   double angle=i*Math.PI/180;
   float dx=(float)Math.Cos(angle)/(extent*0.1f);
   float dy=(float)Math.Sin(angle)/(extent*0.5f);
   float scale=Math.Max(Math.Abs(dx),Math.Abs(dy));
   dx/=scale; dy/=scale;
   float a=dx*dx+dy*dy;
   float exit=(float)Math.Sqrt(a*2.6f*2.6f)/a/scale;
   if(!(a > 1e-12f && exit > 0) || float.IsInfinity(exit))
    throw new Exception("Scale-dependent intersection failure");
   ++cases;
  }
  Console.WriteLine("PASS: {0} interval/intersection cases; maximum {1}/1024 steps; scales through 5 billion units.",cases,maximumSteps);
 }
}
'@
[LargeDustSamplingTest]::Run()
