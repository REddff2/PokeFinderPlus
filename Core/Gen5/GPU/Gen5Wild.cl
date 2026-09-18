// OpenCL C 1.2. Proven White/None/Grass/Spring payload guard; requested legal IV bounds.
// Unsigned overflow is intentional, matching LCRNG64 and MT exactly.
ulong step(ulong *s) { return *s = *s * (ulong)0x5d588b656c078965UL + 0x269ec3UL; }
uint bounded(ulong *s, uint m) { return (uint)(((step(s) >> 32) * m) >> 32); }
ulong startup(ulong seed) {
    // Every counted call in advanceProbabilityTable advances the same stream.
    // Keeping its terminal state equals BWRNG(seed, initialAdvancesBW(seed)).
    for (uint i=0;i<5;++i) {
        step(&seed);
        if (bounded(&seed,101)>50) step(&seed);
        if (bounded(&seed,101)>30) step(&seed);
        if (bounded(&seed,101)>25) { if (bounded(&seed,101)>30) step(&seed); }
        if (bounded(&seed,101)>20) { if (bounded(&seed,101)>25) { if (bounded(&seed,101)>33) step(&seed); } }
    }
    return seed;
}
uint slot_of(uint r) {
    const uint limits[12]={20,40,50,60,70,80,85,90,94,98,99,100};
    uint s=0;while(r>=limits[s])++s;return s;
}
uint pid_final(ulong *s) {
    uint pid=(uint)(step(s)>>32);
    pid ^= 0x10000; // createPID ability=2: always toggles the ability bit.
    if (((69 ^ 58008) ^ pid)&1) pid|=0x80000000; else pid&=0x7fffffff;
    return pid;
}
uint shiny_of(uint pid) {
    uint x=((pid>>16)^(pid&65535)^(69^58008));
    return x==0?2:(x<8?1:0);
}
uint ivs_of(ulong seed) {
    uint low[7];uint x=(uint)(seed>>32);low[0]=x;uint packed=0;
    for(uint i=1;i<=402;++i) {
        x=1812433253U*(x^(x>>30))+i;
        if(i<7)low[i]=x;
        if(i>=397) {
            uint j=i-397;
            uint joined=(low[j]&0x80000000U)|(low[j+1]&0x7fffffffU);
            uint y=x^(joined>>1)^((joined&1)?0x9908b0dfU:0);
            y^=y>>11;y^=(y<<7)&0x9d2c5680U;y^=(y<<15)&0xefc60000U;
            // Upper five bits are unaffected by the final y ^= y >> 18.
            packed|=(y>>27)<<(5*j);
        }
    }
    return packed;
}
uint hp_of(uint p) {
    uint bits=(p&1)|(((p>>5)&1)<<1)|(((p>>10)&1)<<2)|(((p>>25)&1)<<3)|(((p>>15)&1)<<4)|(((p>>20)&1)<<5);
    return bits*15/63;
}
uint iv_pass(uint p, uint ivMin, uint ivMax) {
    // Internal order is HP, Atk, Def, SpA, SpD, Spe, matching CPU IV arrays.
    for(uint i=0;i<6;++i) {
        uint value=(p>>(5*i))&31;
        if(value<((ivMin>>(5*i))&31) || value>((ivMax>>(5*i))&31))return 0;
    }
    return 1;
}
__kernel void compact(__global const ulong *seeds,__global uint *indices,volatile __global uint *count,uint n,uint ivMin,uint ivMax) {
    uint i=get_global_id(0);if(i>=n)return;
    ulong s=startup(seeds[i]);bounded(&s,65535); // Lead None draw
    uint roll=bounded(&s,65535)/656;
    if(roll<70 || roll>=80)return; // Grass slot 5, exact guarded encounter table
    bounded(&s,65535); // Required level draw: fixed level 20 still consumes it
    if(!shiny_of(pid_final(&s)))return;
    if(!iv_pass(ivs_of(seeds[i]),ivMin,ivMax))return;
    // Guard admits Hidden Power Any only. CPU finalizes from original seed.
    indices[atomic_inc(count)]=i;
}
// Oracle mode deliberately computes every field/IV even for rejected candidates.
// Output layout: slot, level draw (0..99), PID, shiny, packed IVs, HP, eligible, reserved.
__kernel void inspect(__global const ulong *seeds,__global uint *out,volatile __global uint *count,uint n,uint ivMin,uint ivMax) {
    uint i=get_global_id(0);if(i>=n)return;
    ulong s=startup(seeds[i]);bounded(&s,65535);
    uint slot=slot_of(bounded(&s,65535)/656);uint draw=bounded(&s,65535)/656;
    uint pid=pid_final(&s),shiny=shiny_of(pid),ivs=ivs_of(seeds[i]);
    out[8*i]=slot;out[8*i+1]=draw;out[8*i+2]=pid;out[8*i+3]=shiny;
    out[8*i+4]=ivs;out[8*i+5]=hp_of(ivs);out[8*i+6]=(slot==5 && shiny && iv_pass(ivs,ivMin,ivMax));out[8*i+7]=0;
}

// Independent final-IV prefilter. No PID, gender, nature, or encounter code.
// MT outputs 0..223 use the unmodified first twist dependency i+397.
uint ivs_at(ulong seed, uint offset, uint roamer) {
    uint low[7]; uint x=(uint)(seed>>32); uint packed=0;
    if(offset==0)low[0]=x;
    for(uint i=1;i<=offset+402;++i) {
        x=1812433253U*(x^(x>>30))+i;
        if(i>=offset && i<=offset+6)low[i-offset]=x;
        if(i>=offset+397) {
            uint j=i-offset-397;
            uint joined=(low[j]&0x80000000U)|(low[j+1]&0x7fffffffU);
            uint y=x^(joined>>1)^((joined&1)?0x9908b0dfU:0);
            y^=y>>11;y^=(y<<7)&0x9d2c5680U;y^=(y<<15)&0xefc60000U;
            uint stat=roamer && j>=3 ? (j==3?4:(j==4?5:3)) : j;
            packed|=(y>>27)<<(5*stat);
        }
    }
    return packed;
}
__kernel void ivCompact(__global const ulong *seeds,__global uint *indices,volatile __global uint *count,uint n,uint ivMin,uint ivMax,uint offset,uint roamer) {
    uint i=get_global_id(0);if(i>=n)return;
    if(iv_pass(ivs_at(seeds[i],offset,roamer),ivMin,ivMax))indices[atomic_inc(count)]=i;
}
__kernel void ivInspect(__global const ulong *seeds,__global uint *out,volatile __global uint *count,uint n,uint ivMin,uint ivMax,uint offset,uint roamer) {
    uint i=get_global_id(0);if(i>=n)return;uint ivs=ivs_at(seeds[i],offset,roamer);
    for(uint j=0;j<8;++j)out[8*i+j]=0;
    out[8*i+4]=ivs;out[8*i+5]=hp_of(ivs);out[8*i+6]=iv_pass(ivs,ivMin,ivMax);
}

uint cache_pass(uint hp,uint atk,uint def,uint spa,uint spd,uint spe,uint roamer) {
    return hp>=30 && def>=30 && spd>=30 && (atk>=30 || spa>=30) && (spe>=30 || (!roamer && spe<=1));
}
__kernel void cacheCompact(__global const ulong *seeds,__global uint *indices,volatile __global uint *count,uint n,uint ivMin,uint ivMax,uint offset,uint frames) {
    uint gid=get_global_id(0);if(gid>=n)return;
    uint low[40],v[39];uint x=(uint)(seeds[gid]>>32);uint outputs=frames+31;
    if(offset==0)low[0]=x;
    for(uint i=1;i<offset+outputs+397;++i) {
        x=1812433253U*(x^(x>>30))+i;
        if(i>=offset && i<=offset+outputs)low[i-offset]=x;
        if(i>=offset+397) {
            uint j=i-offset-397;
            uint joined=(low[j]&0x80000000U)|(low[j+1]&0x7fffffffU);
            uint y=x^(joined>>1)^((joined&1)?0x9908b0dfU:0);
            y^=y>>11;y^=(y<<7)&0x9d2c5680U;y^=(y<<15)&0xefc60000U;v[j]=y>>27;
        }
    }
    for(uint i=0;i<frames+4;++i) {
        uint j=i+22;
        uint pass=cache_pass(v[j],v[j+1],v[j+2],v[j+3],v[j+4],v[j+5],0);
        if(i<frames+2)pass|=cache_pass(v[i],v[i+1],v[i+2],v[i+3],v[i+4],v[i+5],0);
        j=i+1;
        if(i<frames)pass|=cache_pass(v[j],v[j+1],v[j+2],v[j+5],v[j+3],v[j+4],1);
        if(pass){indices[atomic_inc(count)]=gid;return;}
    }
}
