float ReferenceParticleGridValue(uint index)
{
    DustVolume packed = DustVolumes[256u + index / 44u];
    uint lane=index%44u;float4 values=0.0;
    switch(lane/4u) {
        case 0:values=asfloat(uint4(packed.enabled,packed.shape,packed.shapeSeed,packed.flags));break;
        case 1:values=float4(packed.position,packed.density);break;
        case 2:values=float4(packed.size,packed.scattering);break;
        case 3:values=float4(packed.color,packed.absorption);break;
        case 4:values=float4(packed.anisotropy,packed.coverage,packed.noiseScale,packed.noiseDetail);break;
        case 5:values=float4(packed.shapeVariation,packed.rotationDegrees);break;
        case 6:values=float4(packed.rotationXSinCos,packed.rotationYSinCos);break;
        case 7:values=float4(packed.rotationZSinCos,packed.precomputedPadding);break;
        case 8:values=float4(packed.seedOffset,packed.seedPadding);break;
        case 9:values=packed.openmwControl;break;
        default:values=packed.openmwTime;break;
    }
    return values[lane&3u];
}
