param([string]$Source = "$PSScriptRoot\..\src\Modern\ModernGraphics.cpp")
$ErrorActionPreference = 'Stop'
$text = [IO.File]::ReadAllText((Resolve-Path -LiteralPath $Source))
function Require([bool]$condition, [string]$message) {
    if (!$condition) { throw $message }
    Write-Output "PASS: $message"
}
$start = $text.IndexOf('void recordDustScreenBuild(')
$end = $text.IndexOf('void recordScenePrepass(', $start)
Require ($start -ge 0 -and $end -gt $start) 'screen pass located'
$screen = $text.Substring($start, $end-$start)
Require ($screen -notmatch 'constants\.presenterPadding\[1\]\s*=(?!=)') 'screen upload preserves visibility/density mode'
Require ($text.Contains('bool froxelAvailable = PresenterPadding.y == 1.0 && !forceDirect;')) 'visibility field cannot enter density integration'
Require ($text.Contains('constants.presenterPadding[1] = visibilityOnly ? 3.0f : 1.0f;')) 'sun-only dispatch publishes visibility mode'
Require ($text.Contains('DustSegmentSunVisibility(samplePosition,rayDirection,')) 'live cloud lighting integrates visibility separately'
Require ($text.Contains('lighting += directSunLighting * geometryVisibility;')) 'direct cloud sunlight actually applies geometry visibility'
Require ($text.Contains('integrationLength)-1.0)')) 'ship shadows integrated independently of expensive lighting cache'
Require ($text.Contains('ceil(length / DustShadowStepLength(rayDirection))')) 'cloud shadow quadrature resolves light-grid cells'
Require ($text.Contains('step<shadowSteps')) 'shafts use adaptive visibility sampling'
Require (!$text.Contains('step<64u')) 'shafts no longer project a fixed 64 silhouette slices'
Require ($text.Contains('visibilityOnlyGrid ? biasedCenter : std::floor(')) 'sun grid moves continuously while density grid stays snapped'
Require ($text.Contains('farVisibility,smoothstep(0.8,1.0,nearEdge)')) 'near shadow cascade blends into far cascade'
Require ($text.Contains('1.0,smoothstep(0.8,1.0,farEdge)')) 'outer shadow coverage fades continuously'
Require ($text.Contains('if (PresenterPadding.y != 3.0) return 1.0;')) 'visibility sampler rejects density mode'
Require ($text.Contains('float3 AtmosphericSunScattering(')) 'standalone atmosphere exists'
Require ($text.Contains('if (DustRenderEnabled != 0u)')) 'presenter accepts zero-cloud atmosphere'
Require (!$text.Contains('DustRenderEnabled != 0u && DustVolumeCount > 0u')) 'presenter has no authored-cloud gate'
Require (!$text.Contains('constants.dustRenderEnabled == 0u || constants.dustVolumeCount == 0u')) 'visibility and screen dispatches accept zero clouds'
Require ($text.Contains('volumeCount > 0 || atmosphereRequested ? 1u : 0u')) 'CPU enables zero-cloud atmosphere'
$atmosphereStart = $text.IndexOf('float3 AtmosphericSunScattering(')
$atmosphereEnd = $text.IndexOf('float3 ApplyVolumetricDust(', $atmosphereStart)
$atmosphere = $text.Substring($atmosphereStart,$atmosphereEnd-$atmosphereStart)
Require (!$atmosphere.Contains('SkyToDisplay(')) 'atmosphere stays linear until scene composition'
Require (!$atmosphere.Contains('(0.035+phase)')) 'no omnidirectional haze pedestal'
Require ($atmosphere.Contains('0.18 * sourceResponse * max(GodRays,0.0)')) 'mission beam strength scales independently of authored HDR brightness'
Require ($atmosphere.Contains('GodRaySourceExternal & 4u')) 'mission-specific ray tint supported'
Require ($atmosphere.Contains('max(0.0,visibility-reference)')) 'uniform sunlight adds zero whole-screen haze'
Require (!$atmosphere.Contains('sceneDistance),20000.0')) 'shaft integration no longer stops before a zoomed-out fleet'
Require ($atmosphere.Contains('entry+((float)step+0.5)*stepLength')) 'shaft samples start at the fleet-region intersection'
Require ($atmosphere.Contains('smoothstep(0.65,1.0,length(p-center)/radius)')) 'scattering region has no hard box boundary'
Require ($text.Contains('visibilityOnlyGrid ? visibilityCellSize : finestRequestedCellSize')) 'fine sun grid does not change authored cloud density resolution'
Require ($text.Contains('? presenter.godRayFocus[axis]')) 'sun grid follows the world-space orbit target'
Require ($text.Contains('void BuildDustSunColumnsCS(')) 'sun shadows use straight light-aligned columns'
Require ($text.Contains('float3 local = float3(dot(offset,u),dot(offset,v),')) 'visibility lookup uses the same light-aligned basis'
Require ($text.Contains('range-query.CommittedRayT()')) 'columns use nearest surface from the sunlight side'
Require ($text.Contains('HW_CAPTURE_SUN_SHAFTS')) 'expensive GPU captures are opt-in'
Require (!$atmosphere.Contains('coneFade')) 'no circular per-pixel cone cutoff'
Require (!$text.Contains('float SunShaftSourceVisibility()')) 'covering the sun does not globally suppress surrounding shafts'
Require ($atmosphere.Contains('DustProceduralSunVisibility(p,true)')) 'shaft occlusion remains local with separate asteroid exemptions'
Require ($text.Contains('shaftQuery.TraceRayInline(DustShadowScene,RAY_FLAG_NONE,0x04u,ray);')) 'shafts have a separate solid-blocker mask'
Require ($text.Contains('float4(shaftVisibility,0,0,visibility)')) 'cloud shadows preserve all blockers independently of shafts'
Require (!$text.Contains('GodRaySourceExternal & 8u')) 'cloud shadow lighting is not gated by god rays'
Require ($text.Contains('|| atmosphereRequested || cloudShadowsRequested')) 'cloud shadow grid is built with god rays disabled'
Require ($text.Contains('constants.rayOverlayEnabled && constants.dustSunDirectionValid[3] > 0.5f')) 'cloud shadow TLAS is available with god rays disabled'
Require ($text.Contains('volumetricDustScratch.back().enabled |= 2u;')) 'harvestable native-resolution tag is runtime-only'
Require ($text.Contains('ApplyVolumetricDust(uv,0.0,true,transmission)')) 'harvestable coverage resolves the complete medium at native resolution'
Require ($text.Contains('if (forceDirect) SceneDepth.GetDimensions(dustWidth, dustHeight);')) 'native resolve uses native pixel footprint rather than quarter-resolution footprint'
