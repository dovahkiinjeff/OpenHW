    uint remaining[8];
    [unroll] for(uint word=0u;word<8u;++word)remaining[word]=DustTileMasks[tileBase+word];
    [loop]
    for (uint candidateIndex = 0u; candidateIndex < DustVolumeCount; ++candidateIndex)
    {
        // Composite bricks in this pixel ray's entry order, independent of
        // camera-distance sorting and the packed density-grid offsets.
        uint volumeIndex=0xffffffffu;
        float nearestEntry=3.0e30,nearestExit=0.0;
        [loop] for(uint word=0u;word<8u;++word){
            uint pending=remaining[word];
            [loop] while(pending!=0u){
                uint bit=firstbitlow(pending),index=word*32u+bit;
                pending &= pending-1u;
                if(index>=DustVolumeCount){remaining[word]&=~(1u<<bit);continue;}
                DustVolume candidate=DustVolumes[index];float entry,exit;
                if(candidate.enabled==0u||candidate.density<=0.00001||
                   !DustRayVolumeInterval(candidate,DustBuildRotationContext(candidate),rayOrigin,rayDirection,entry,exit)||
                   min(sceneDistance,exit)<=max(0.0,entry)){
                    remaining[word]&=~(1u<<bit);continue;
                }
                float clippedEntry=max(0.0,entry);
                if(clippedEntry<nearestEntry || (clippedEntry==nearestEntry&&exit<nearestExit)){
                    nearestEntry=clippedEntry;nearestExit=exit;volumeIndex=index;
                }
            }
        }
        if(volumeIndex==0xffffffffu)break;
        remaining[volumeIndex/32u]&=~(1u<<(volumeIndex%32u));
        DustVolume volume = DustVolumes[volumeIndex];
        if (volume.enabled == 0u || volume.density <= 0.00001 ||
            transmittance < 0.008) continue;
        /* Rotation and hash-derived phase are invariant for every ray sample
           in this volume. RTX-AAA computes them once instead of repeating
           trigonometry/hash work in density and shadow probes. */
        DustRotationContext rotation = DustBuildRotationContext(volume);
        // Reject empty/geometry-occluded support intervals before evaluating
        // distance fade, material cadence, noise phase or lighting invariants.
        float volumeEntryDistance;
        float volumeExitDistance;
        if (!DustRayVolumeInterval(volume, rotation, rayOrigin, rayDirection,
                                   volumeEntryDistance, volumeExitDistance))
            continue;
        float startDistance = max(
            froxelAvailable ? nearExitDistance : 0.0,
            max(0.0, volumeEntryDistance));
        float endDistance = min(sceneDistance, volumeExitDistance);
        if (endDistance <= startDistance) continue;
