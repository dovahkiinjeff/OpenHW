#ifndef PARTICLE_DENSITY_FIELD_H
#define PARTICLE_DENSITY_FIELD_H
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define PD_SIDE 17
#define PD_VALUES (PD_SIDE*PD_SIDE*PD_SIDE)
#define PD_RADIUS 8000.0f
#define PD_HASH 65536
#define PD_MAX_TILES 16384
typedef struct PDPoint { float p[3]; int group; float spacing,support; } PDPoint;
typedef struct PDTile {int group,cell[3],next;float minimum[3],span,peak;} PDTile;
typedef struct PDField {PDTile *tiles;float *values;int count,capacity,overflow;} PDField;
static unsigned pdHash(int x,int y,int z,int group)
{return ((unsigned)x*73856093u^(unsigned)y*19349663u^(unsigned)z*83492791u^(unsigned)group*2654435761u)&(PD_HASH-1);}
static int pdCompare(const void *a,const void *b)
{int k;const PDPoint *p=a,*q=b;for(k=0;k<3;k++){if(p->p[k]<q->p[k])return -1;if(p->p[k]>q->p[k])return 1;}return 0;}
static int pdRoot(int *parent,int x)
{while(parent[x]!=x){parent[x]=parent[parent[x]];x=parent[x];}return x;}
/* Balanced spatial tree: eight nearest neighbours establish the sampling
   scale locally, rather than assuming every map uses the same spacing. */
static const PDPoint *pdSortPoints;
static int pdSortAxis;
static int pdIndexCompare(const void *a,const void *b)
{
    int i=*(const int *)a,j=*(const int *)b;
    float d=pdSortPoints[i].p[pdSortAxis]-pdSortPoints[j].p[pdSortAxis];
    return d<0?-1:d>0?1:i<j?-1:i>j;
}
static void pdTree(int *indices,int n,const PDPoint *p,int depth)
{
    int middle=n/2;if(n<2)return;
    pdSortPoints=p;pdSortAxis=depth%3;qsort(indices,n,sizeof(int),pdIndexCompare);
    pdTree(indices,middle,p,depth+1);pdTree(indices+middle+1,n-middle-1,p,depth+1);
}
typedef struct PDNear {float d[8];int id[8],count;} PDNear;
static void pdNearest(const PDPoint *p,const int *tree,int n,int query,int depth,PDNear *near)
{
    int middle=n/2,id,k;float delta,d2=0;
    if(!n)return;id=tree[middle];
    if(id!=query){
        for(k=0;k<3;k++){float d=p[id].p[k]-p[query].p[k];d2+=d*d;}
        if(near->count<8||d2<near->d[7]){
            k=near->count<8?near->count++:7;
            while(k>0&&near->d[k-1]>d2){near->d[k]=near->d[k-1];near->id[k]=near->id[k-1];k--;}
            near->d[k]=d2;near->id[k]=id;
        }
    }
    delta=p[query].p[depth%3]-p[id].p[depth%3];
    if(delta<=0){
        pdNearest(p,tree,middle,query,depth+1,near);
        if(near->count<8||delta*delta<=near->d[7])pdNearest(p,tree+middle+1,n-middle-1,query,depth+1,near);
    }else{
        pdNearest(p,tree+middle+1,n-middle-1,query,depth+1,near);
        if(near->count<8||delta*delta<=near->d[7])pdNearest(p,tree,middle,query,depth+1,near);
    }
}
static void pdForeignDistance(const PDPoint *p,const int *tree,int n,int query,int depth,float *limit)
{
    int middle=n/2,id,k;float delta,d2=0;
    if(!n)return;id=tree[middle];
    if(p[id].group!=p[query].group){
        for(k=0;k<3;k++){float d=p[id].p[k]-p[query].p[k];d2+=d*d;}
        if(d2<*limit)*limit=d2;
    }
    delta=p[query].p[depth%3]-p[id].p[depth%3];
    if(delta<=0){
        pdForeignDistance(p,tree,middle,query,depth+1,limit);
        if(delta*delta<*limit)pdForeignDistance(p,tree+middle+1,n-middle-1,query,depth+1,limit);
    }else{
        pdForeignDistance(p,tree+middle+1,n-middle-1,query,depth+1,limit);
        if(delta*delta<*limit)pdForeignDistance(p,tree,middle,query,depth+1,limit);
    }
}
typedef struct PDFormation {float center[3],variance,spacing,reach;int count,compact;} PDFormation;
static int pdFloatCompare(const void *a,const void *b)
{float x=*(const float *)a,y=*(const float *)b;return x<y?-1:x>y;}
static int pdTreeGroups(const PDPoint *p,const int *tree,int n,int *uniform)
{
    int middle=n/2,g;if(!n)return -2;g=p[tree[middle]].group;
    if(middle&&pdTreeGroups(p,tree,middle,uniform)!=g)g=-1;
    if(n-middle-1&&pdTreeGroups(p,tree+middle+1,n-middle-1,uniform+middle+1)!=p[tree[middle]].group)g=-1;
    uniform[middle]=g;return g;
}
static void pdJoinFormation(const PDPoint *p,const int *tree,int n,int query,int depth,
    const PDFormation *formations,int *parent,const int *uniform)
{
    int middle=n/2,id,k,a,b;float delta,d2=0,reach;
    if(!n||uniform[middle]==p[query].group)return;id=tree[middle];
    reach=formations[p[query].group].reach;
    if(p[id].group!=p[query].group&&formations[p[id].group].compact==formations[p[query].group].compact){
        float mutual=fminf(reach,formations[p[id].group].reach);
        for(k=0;k<3;k++){float d=p[id].p[k]-p[query].p[k];d2+=d*d;}
        if(d2<=mutual*mutual){
            a=pdRoot(parent,query);b=pdRoot(parent,id);
            if(a!=b){if(a<b)parent[b]=a;else parent[a]=b;}
        }
    }
    delta=p[query].p[depth%3]-p[id].p[depth%3];
    if(delta<=0){
        pdJoinFormation(p,tree,middle,query,depth+1,formations,parent,uniform);
        if(delta*delta<=reach*reach)pdJoinFormation(p,tree+middle+1,n-middle-1,query,depth+1,formations,parent,uniform+middle+1);
    }else{
        pdJoinFormation(p,tree+middle+1,n-middle-1,query,depth+1,formations,parent,uniform+middle+1);
        if(delta*delta<=reach*reach)pdJoinFormation(p,tree,middle,query,depth+1,formations,parent,uniform);
    }
}
/* Mutual local reach follows a branching formation but prevents a sparse
   particle from bridging two dense, well separated clouds. A sharp jump in
   neighbour distance marks the boundary of a small isolated patch. */
static int pdCluster(PDPoint *points,int count,int maxGroups)
{
    int *parent=malloc((size_t)count*sizeof(int)),*tree=malloc((size_t)count*sizeof(int));
    int *groups=malloc((size_t)count*sizeof(int));
    PDNear *neighbours=calloc((size_t)count,sizeof(*neighbours));
    int i,k,groupCount=0,compactCount=0;
    PDFormation *formations=NULL;float *scales=NULL,formationScale;
    if(!count){free(parent);free(tree);free(groups);free(neighbours);return 0;}
    if(!parent||!tree||!groups||!neighbours){free(parent);free(tree);free(groups);free(neighbours);return -1;}
    qsort(points,(size_t)count,sizeof(*points),pdCompare);
    for(i=0;i<count;i++){tree[i]=parent[i]=i;groups[i]=-1;}
    pdTree(tree,count,points,0);
    for(i=0;i<count;i++){
        PDNear *near=&neighbours[i];float previous=0,scale=0;
        pdNearest(points,tree,count,i,0,near);
        for(k=0;k<near->count;k++){
            float distance=sqrtf(near->d[k]);
            if(distance<1.0f)continue; /* coincident particles still add mass */
            if(previous>0&&distance>previous*2.5f)break;
            scale=previous=distance;
        }
        points[i].spacing=fmaxf(1.0f,scale);
    }
    for(i=0;i<count;i++)for(k=0;k<neighbours[i].count;k++){
        int j=neighbours[i].id[k],a,b;
        float reach=1.8f*fminf(points[i].spacing,points[j].spacing);
        if(neighbours[i].d[k]>reach*reach)continue;
        a=pdRoot(parent,i);b=pdRoot(parent,j);if(a!=b){if(a<b)parent[b]=a;else parent[a]=b;}
    }
    /* The nearest-neighbour graph finds sampling patches, not whole clouds.
       Join those patches at formation scale. A compact isolated cloud has a
       small mutual reach even beside a very large branching belt. Compute
       reach once, before joins, so merging cannot inflate it across gaps. */
    formations=calloc((size_t)count,sizeof(*formations));
    scales=malloc((size_t)count*sizeof(float));
    if(!formations||!scales){free(formations);free(scales);free(parent);free(tree);free(groups);free(neighbours);return -1;}
    for(i=0;i<count;i++)scales[i]=points[i].spacing;
    qsort(scales,count,sizeof(float),pdFloatCompare);
    formationScale=scales[(count-1)*3/4];free(scales);
    for(i=0;i<count;i++){
        int root=pdRoot(parent,i);PDFormation *f=&formations[root];
        points[i].group=root;f->count++;f->spacing+=points[i].spacing;
        for(k=0;k<3;k++)f->center[k]+=points[i].p[k];
    }
    for(i=0;i<count;i++)if(formations[i].count){
        for(k=0;k<3;k++)formations[i].center[k]/=formations[i].count;
        formations[i].spacing/=formations[i].count;
    }
    for(i=0;i<count;i++){
        PDFormation *f=&formations[points[i].group];
        for(k=0;k<3;k++){float d=points[i].p[k]-f->center[k];f->variance+=d*d;}
    }
    for(i=0;i<count;i++)if(formations[i].count){
        PDFormation *f=&formations[i];float extent=sqrtf(f->variance/f->count);
        int compact=f->count>=12&&extent<formationScale*.35f&&f->spacing<formationScale*.35f;
        f->compact=compact;
        f->reach=fmaxf(3*f->spacing,1.5f*extent);
        /* Sparse stretches must use a formation-scale sampling distance.
           Dense compact islands retain their smaller mutual reach. */
        if(count>=32&&!compact)f->reach=fmaxf(f->reach,5.0f*formationScale);
    }
    /* Tiny pairs near an isolated dense island belong to that island. They
       must not borrow a sparse-belt reach and bridge the island to the belt. */
    for(i=0;i<count;i++)if(formations[i].compact)groups[compactCount++]=i;
    for(i=0;i<count;i++)if(formations[i].count&&formations[i].count<6){
        int j,entry,best=-1;float bestDistance=1e30f;
        for(entry=0;entry<compactCount;entry++){
            j=groups[entry];
            float d2=0;for(k=0;k<3;k++){float d=formations[i].center[k]-formations[j].center[k];d2+=d*d;}
            if(d2<bestDistance&&d2<2.25f*formations[j].reach*formations[j].reach){best=j;bestDistance=d2;}
        }
        if(best>=0)parent[i]=best;
    }
    for(i=0;i<count;i++)points[i].group=pdRoot(parent,i);
    pdTreeGroups(points,tree,count,groups);
    for(i=0;i<count;i++)pdJoinFormation(points,tree,count,i,0,formations,parent,groups);
    memset(groups,255,(size_t)count*sizeof(int));
    for(i=0;i<count;i++){
        int root=pdRoot(parent,i);
        if(groups[root]<0)groups[root]=groupCount++;
        points[i].group=groups[root]<maxGroups?groups[root]:-1;
    }
    free(formations);free(parent);free(tree);free(groups);free(neighbours);return groupCount;
}
static float pdKernel(float distanceSquared,float radiusSquared)
{float t=1-distanceSquared/radiusSquared;return t>0?t*t*t:0;}
static void pdFree(PDField *f)
{free(f->tiles);free(f->values);memset(f,0,sizeof(*f));}
static int pdNewTile(PDField *f)
{
    int n=f->count;
    if(n>=PD_MAX_TILES){f->overflow=1;return -1;}
    if(n==f->capacity){
        int cap=f->capacity?f->capacity*2:128;
        PDTile *tiles=realloc(f->tiles,(size_t)cap*sizeof(*tiles));
        float *values;
        if(!tiles)return -1;f->tiles=tiles;
        values=realloc(f->values,(size_t)cap*PD_VALUES*sizeof(float));
        if(!values)return -1;f->values=values;f->capacity=cap;
    }
    memset(&f->tiles[n],0,sizeof(PDTile));memset(f->values+(size_t)n*PD_VALUES,0,PD_VALUES*sizeof(float));f->count++;return n;
}
/* Bake sparse bricks of the same world-space kernel sum. Bricks share samples
   at their borders; their bounds are storage/culling only, never cloud shapes. */
static int pdBakeScaled(PDPoint *points,int count,const float *spread,const float *softness,int groupCount,PDField *f,const float *bodyRadius)
{
    int *buckets=malloc(PD_HASH*sizeof(int)),i,k;
    pdFree(f);if(!buckets)return 0;memset(buckets,255,PD_HASH*sizeof(int));
    for(i=0;i<count;i++){
        int g=points[i].group,lo[3],hi[3],x,y,z;
        float radius,span,spacing;
        if(g<0||g>=groupCount||spread[g]<=0)continue;
        radius=(bodyRadius?points[i].support:PD_RADIUS)*spread[g];
        span=4*(bodyRadius?2*bodyRadius[g]:PD_RADIUS)*spread[g];spacing=span/(PD_SIDE-1);
        for(k=0;k<3;k++){lo[k]=(int)floorf((points[i].p[k]-radius)/span);hi[k]=(int)floorf((points[i].p[k]+radius)/span);}
        for(z=lo[2];z<=hi[2];z++)for(y=lo[1];y<=hi[1];y++)for(x=lo[0];x<=hi[0];x++){
            unsigned h=pdHash(x,y,z,g);int tile=buckets[h],q[3]={x,y,z},a,b,c;
            PDTile *t;float *values;int start[3],end[3];
            while(tile>=0){t=&f->tiles[tile];if(t->group==g&&!memcmp(t->cell,q,sizeof(q)))break;tile=t->next;}
            if(tile<0){
                tile=pdNewTile(f);if(tile<0){free(buckets);return 0;}
                t=&f->tiles[tile];t->group=g;t->span=span;t->next=buckets[h];buckets[h]=tile;memcpy(t->cell,q,sizeof(q));
                for(k=0;k<3;k++)t->minimum[k]=q[k]*span;
            }
            t=&f->tiles[tile];values=f->values+(size_t)tile*PD_VALUES;
            for(k=0;k<3;k++){
                start[k]=(int)ceilf((points[i].p[k]-radius-t->minimum[k])/spacing);if(start[k]<0)start[k]=0;
                end[k]=(int)floorf((points[i].p[k]+radius-t->minimum[k])/spacing);if(end[k]>=PD_SIDE)end[k]=PD_SIDE-1;
            }
            for(c=start[2];c<=end[2];c++)for(b=start[1];b<=end[1];b++)for(a=start[0];a<=end[0];a++){
                float d2=0;int sample[3]={a,b,c},index=(c*PD_SIDE+b)*PD_SIDE+a;
                for(k=0;k<3;k++){float d=t->minimum[k]+sample[k]*spacing-points[i].p[k];d2+=d*d;}
                {float kernel=pdKernel(d2,radius*radius);
                 if(kernel>0 && softness)kernel=powf(kernel,(1.0f+2.0f*softness[g])/3.0f);
                 /* Spread shapes support, not inverse-cube brightness. Keep
                    compact patches finite and soften high-count hotspots. */
                 values[index]+=kernel;if(values[index]>t->peak)t->peak=values[index];}
            }
        }
    }
    if(bodyRadius)for(i=0;i<f->count;i++){
        float *values=f->values+(size_t)i*PD_VALUES;f->tiles[i].peak=0;
        for(k=0;k<PD_VALUES;k++){
            values[k]=log1pf(values[k]);
            if(values[k]>f->tiles[i].peak)f->tiles[i].peak=values[k];
        }
    }
    free(buckets);return 1;
}
static int pdBake(PDPoint *points,int count,const float *spread,const float *softness,int groupCount,PDField *f)
{return pdBakeScaled(points,count,spread,softness,groupCount,f,NULL);}
/* A formation-wide upper-quartile bandwidth reconstructs sparse sampling gaps.
   Local bandwidth can expand in sparse branches. Log density limits hotspots;
   no bounding sphere/box fills the empty space between separate formations. */
static int pdBakeClouds(PDPoint *points,int count,const float *spread,const float *softness,int groups,PDField *f)
{
    float *radii=calloc((size_t)groups,sizeof(float)),*samples=malloc((size_t)count*sizeof(float));
    int g,i,n,result,*tree=malloc((size_t)count*sizeof(int));
    if(!radii||!samples||!tree){free(radii);free(samples);free(tree);return 0;}
    for(g=0;g<groups;g++){
        n=0;for(i=0;i<count;i++)if(points[i].group==g)samples[n++]=points[i].spacing;
        if(n){qsort(samples,n,sizeof(float),pdFloatCompare);radii[g]=fmaxf(500.0f,3*samples[(n-1)*3/4]);}
    }
    for(i=0;i<count;i++)tree[i]=i;pdTree(tree,count,points,0);
    for(i=0;i<count;i++){
        float radius,limit;g=points[i].group;if(g<0||g>=groups)continue;
        radius=fmaxf(radii[g],fminf(2*radii[g],2*points[i].spacing));
        /* Keep reconstruction support on its own side of an inter-cloud gap,
           including when the artist increases Spread. */
        limit=radius*fmaxf(1.0f,spread[g])/.45f;limit*=limit;
        if(groups>1)pdForeignDistance(points,tree,count,i,0,&limit);
        points[i].support=fminf(radius,.45f*sqrtf(limit)/fmaxf(1.0f,spread[g]));
    }
    result=pdBakeScaled(points,count,spread,softness,groups,f,radii);free(radii);free(samples);free(tree);return result;
}
static float pdSample(const PDField *f,int tile,const float p[3])
{
    const PDTile *t=&f->tiles[tile];const float *values=f->values+(size_t)tile*PD_VALUES;
    int base[3],k,x,y,z;float fraction[3],result=0;
    for(k=0;k<3;k++){
        float q=(p[k]-t->minimum[k])*(PD_SIDE-1)/t->span;
        if(q<0||q>PD_SIDE-1)return 0;
        base[k]=(int)floorf(q);if(base[k]>PD_SIDE-2)base[k]=PD_SIDE-2;fraction[k]=q-base[k];
    }
    for(z=0;z<2;z++)for(y=0;y<2;y++)for(x=0;x<2;x++)
        result+=values[((base[2]+z)*PD_SIDE+base[1]+y)*PD_SIDE+base[0]+x]*(x?fraction[0]:1-fraction[0])*(y?fraction[1]:1-fraction[1])*(z?fraction[2]:1-fraction[2]);
    return result;
}
#endif
