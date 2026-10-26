param([string]$Source = "$PSScriptRoot\..\src\Modern\ModernGraphics.cpp", [switch]$Legacy, [string]$CacheDirectory)
$ErrorActionPreference = 'Stop'
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class PresenterShaderValidation {
 public static string CacheDirectory;
 [DllImport("d3dcompiler_47.dll", CharSet=CharSet.Ansi)]
 static extern int D3DCompile(byte[] src, UIntPtr length, string name, IntPtr macros,
   IntPtr include, string entry, string target, uint flags, uint flags2,
   out IntPtr code, out IntPtr errors);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)]
 delegate IntPtr GetPointer(IntPtr self);
 [UnmanagedFunctionPointer(CallingConvention.StdCall)]
 delegate UIntPtr GetSize(IntPtr self);
 public static void Compile(string source, string entry, string target) {
   var bytes = System.Text.Encoding.UTF8.GetBytes(source);
   IntPtr code, errors;
   int hr = D3DCompile(bytes, (UIntPtr)bytes.Length, "PresenterValidation", IntPtr.Zero,
     IntPtr.Zero, entry, target, (1u << 11) | (1u << 15), 0, out code, out errors);
   string message = "";
   if (errors != IntPtr.Zero) {
     var table = Marshal.ReadIntPtr(errors);
     var get = Marshal.GetDelegateForFunctionPointer<GetPointer>(Marshal.ReadIntPtr(table, 3 * IntPtr.Size));
     message = Marshal.PtrToStringAnsi(get(errors));
     Marshal.Release(errors);
   }
   if (code != IntPtr.Zero) {
     if (hr >= 0 && !String.IsNullOrEmpty(CacheDirectory)) {
       var table = Marshal.ReadIntPtr(code);
       var get = Marshal.GetDelegateForFunctionPointer<GetPointer>(Marshal.ReadIntPtr(table,3*IntPtr.Size));
       var size = Marshal.GetDelegateForFunctionPointer<GetSize>(Marshal.ReadIntPtr(table,4*IntPtr.Size));
       var compiled = new byte[checked((int)size(code).ToUInt64())];
       Marshal.Copy(get(code),compiled,0,compiled.Length);
       ulong hash=1469598103934665603UL;
       foreach(var segment in new[]{source,entry,target,"HomeworldModern-0.91.2"})
         foreach(byte value in System.Text.Encoding.UTF8.GetBytes(segment))
           unchecked{hash=(hash^value)*1099511628211UL;}
       System.IO.Directory.CreateDirectory(CacheDirectory);
       var path=System.IO.Path.Combine(CacheDirectory,"Presenter-"+entry+"-"+target+"-"+hash.ToString("x16")+".cso");
       System.IO.File.WriteAllBytes(path,compiled);
     }
     Marshal.Release(code);
   }
   if (hr < 0) throw new Exception(entry + ": " + message);
   Console.WriteLine(entry + ": PASS" + (message.Length > 0 ? "\n" + message : ""));
 }
}
'@
if ($Legacy) { $Source = "$PSScriptRoot\..\src\Modern\LegacyD3D12GL.cpp" }
$text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Source))
$marker = 'const char presenterShader[] = R"('
if ($Legacy) { $marker = 'const char *legacyShader = R"(' }
$start = $text.IndexOf($marker) + $marker.Length
if ($start -lt $marker.Length) { throw 'Presenter source block not found' }
$end = $text.IndexOf(')";', $start)
$shader = $text.Substring($start, $end - $start)
$shader = $shader.Replace("`r`n", "`n")
[PresenterShaderValidation]::CacheDirectory = $CacheDirectory
if ($Legacy) {
    [PresenterShaderValidation]::Compile($shader, 'LegacyVS', 'vs_5_0')
    [PresenterShaderValidation]::Compile($shader, 'LegacyPS', 'ps_5_0')
    return
}
[PresenterShaderValidation]::Compile($shader, 'PresentVS', 'vs_5_0')
[PresenterShaderValidation]::Compile($shader, 'PresentPS', 'ps_5_0')
[PresenterShaderValidation]::Compile($shader, 'DustScreenPS', 'ps_5_0')
[PresenterShaderValidation]::Compile("#define DUST_FROXEL_COMPUTE 1`n$shader", 'BuildDustFroxelCS', 'cs_5_0')
