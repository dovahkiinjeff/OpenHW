#include "../src/Game/ModernParticleFields.c"
#include <stdio.h>
Universe universe;
static HWModernVolumetricDustVolume submitted[PD_MAX_TILES];
static float *submittedDensity;
static unsigned submittedCount,submissions;
void hwModernGraphicsSetParticleFieldVolumes(const HWModernVolumetricDustVolume *v,unsigned count)
{submittedCount=count;submissions++;if(count)memcpy(submitted,v,count*sizeof(*v));}
void hwModernGraphicsSetParticleFieldDensity(const float *v,unsigned count)
{free(submittedDensity);submittedDensity=NULL;if(count){submittedDensity=malloc(count*sizeof(float));memcpy(submittedDensity,v,count*sizeof(float));}}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static int tileContaining(const PDField *f,float x,float y,float z)
{
    int i;float p[3]={x,y,z};
    for(i=0;i<f->count;i++){
        const PDTile *t=&f->tiles[i];
        if(p[0]>=t->minimum[0]&&p[0]<t->minimum[0]+t->span&&p[1]>=t->minimum[1]&&p[1]<t->minimum[1]+t->span&&p[2]>=t->minimum[2]&&p[2]<t->minimum[2]+t->span)return i;
    }return -1;
}
int main(int argc,char **argv)
{
    if(argc>1){
        FILE *input=fopen(argv[1],"r");PDPoint *sample=calloc(65536,sizeof(PDPoint));int n=0,result;
        CHECK(input&&sample);
        while(n<65536&&fscanf(input,"%f %f %f",&sample[n].p[0],&sample[n].p[1],&sample[n].p[2])==3)n++;
        fclose(input);result=pdCluster(sample,n,65536);printf("%d particles: %d formations\n",n,result);
        if(result>1){int g,i,k;for(g=0;g<result;g++){int total=0;float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};for(i=0;i<n;i++)if(sample[i].group==g){total++;for(k=0;k<3;k++){lo[k]=fminf(lo[k],sample[i].p[k]);hi[k]=fmaxf(hi[k],sample[i].p[k]);}}printf("  %d: %d particles; (%g %g %g)-(%g %g %g)\n",g,total,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);}}
        free(sample);
        return argc>2&&result!=atoi(argv[2])?1:0;
    }
    {
        /* Small bandwidths previously amplified density by radius^-3.
           Even coincident points now remain bounded at every Spread. */
        PDPoint knots[64]={0};PDField cloud={0};float width[1],soft[1]={.89f};int i,pass;
        for(i=0;i<64;i++){knots[i].p[0]=knots[i].p[1]=knots[i].p[2]=8000;knots[i].spacing=1000;}
        for(pass=0;pass<3;pass++){
            width[0]=pass==0?.25f:pass==1?1:4;
            CHECK(pdBakeClouds(knots,64,width,soft,1,&cloud));
            for(i=0;i<cloud.count*PD_VALUES;i++)CHECK(isfinite(cloud.values[i])&&cloud.values[i]<=log1pf(64)+.00001f);
            pdFree(&cloud);
        }
    }
    /* Fifty dense sampling packets along one belt are ONE editor field.
       Eight-neighbour connectivity alone incorrectly reports 52 fields. */
    {
        PDPoint packets[624];int n=0,packet,j,a,b;
        for(packet=0;packet<50;packet++)for(j=0;j<12;j++){
            PDPoint q={{0,0,0},0,0,0};
            q.p[0]=packet*6000+(j%3)*1000;q.p[1]=(j/3)*1000;
            packets[n++]=q;
        }
        for(packet=0;packet<2;packet++)for(j=0;j<12;j++){
            PDPoint q={{0,0,0},0,0,0};
            q.p[0]=12000+(j%3)*200;q.p[1]=20000+packet*5000+(j/3)*200;
            packets[n++]=q;
        }
        CHECK(pdCluster(packets,n,128)==3);
        for(a=0;a<n;a++)for(b=a+1;b<n;b++)
            if(packets[a].p[1]<4000&&packets[b].p[1]<4000)CHECK(packets[a].group==packets[b].group);
    }
    /* One long, branching formation plus two dense isolated patches. The
       same topology must survive a change in map scale and input order. */
    {
        PDPoint formation[150];int n=0,branch,step,pass;float scale;
        for(branch=0;branch<3;branch++)for(step=0;step<35;step++){
            PDPoint q={{0,0,0},0,0};
            q.p[0]=(branch==0?-1.0f:1.0f)*step*7000;
            q.p[1]=branch==2?step*5500:0;q.p[2]=(step%3)*600;
            formation[n++]=q;
        }
        for(branch=0;branch<2;branch++)for(step=0;step<12;step++){
            PDPoint q={{0,0,0},0,0};q.p[0]=45000+branch*18000+(step%3)*600;
            q.p[1]=-45000+(step/3)*600;formation[n++]=q;
        }
        for(pass=0;pass<2;pass++){
            PDPoint copy[150];int a,b,c;scale=pass?0.03125f:1.0f;
            for(a=0;a<n;a++){copy[a]=formation[n-1-a];for(b=0;b<3;b++)copy[a].p[b]*=scale;}
            CHECK(pdCluster(copy,n,16)==3);
            a=b=c=0;for(step=0;step<n;step++){a+=copy[step].group==0;b+=copy[step].group==1;c+=copy[step].group==2;}
            CHECK(a==105);CHECK(b==12);CHECK(c==12);
            if(!pass){
                PDField body={0};float widths[3]={1,1,1};float midpoint[3]={-101500,0,600};int t;
                CHECK(pdBakeClouds(copy,n,widths,NULL,3,&body));
                t=tileContaining(&body,midpoint[0],midpoint[1],midpoint[2]);CHECK(t>=0);
                CHECK(pdSample(&body,t,midpoint)>0); /* continuous between belt samples */
                pdFree(&body);
                widths[0]=widths[1]=widths[2]=4;
                CHECK(pdBakeClouds(copy,n,widths,NULL,3,&body));
                for(a=0;a<n;a++)for(b=a+1;b<n;b++)if(copy[a].group!=copy[b].group){
                    float d2=0;for(c=0;c<3;c++){float d=copy[a].p[c]-copy[b].p[c];d2+=d*d;}
                    CHECK(4*(copy[a].support+copy[b].support)<sqrtf(d2));
                }
                pdFree(&body);
            }
        }
    }
    PDPoint p[4]={{{4000,4000,4000},0},{{6000,4000,4000},0},{{100000,4000,4000},0},{{100000,4000,4000},0}};
    PDField f={0};float spread[4]={1,1,1,1},at[3]={4000,4000,4000};int groups,tile,i;
    groups=pdCluster(p,4,4);CHECK(groups==2);CHECK(p[0].group==p[1].group);CHECK(p[2].group!=p[0].group);
    CHECK(pdBake(p,4,spread,NULL,groups,&f));tile=tileContaining(&f,4000,4000,4000);CHECK(tile>=0);
    CHECK(fabsf(pdSample(&f,tile,at)-(1+pdKernel(4000000,64000000)))<.00001f);
    at[0]=100000;tile=tileContaining(&f,100000,4000,4000);CHECK(tile>=0);
    CHECK(fabsf(pdSample(&f,tile,at)-2)<.00001f); /* double particle count, double density */
    CHECK(tileContaining(&f,50000,4000,4000)<0); /* actual empty gap remains clear */
    pdFree(&f);
    {PDPoint seam[2]={{{30000,4000,4000},0},{{34000,4000,4000},0}};int left,right;
     CHECK(pdCluster(seam,2,4)==1);CHECK(pdBake(seam,2,spread,NULL,1,&f));
     left=tileContaining(&f,31999,4000,4000);right=tileContaining(&f,32001,4000,4000);CHECK(left>=0&&right>=0&&left!=right);
     at[0]=32000;CHECK(fabsf(pdSample(&f,left,at)-pdSample(&f,right,at))<.00001f);
     CHECK(fabsf(pdSample(&f,left,at)-2*pdKernel(4000000,64000000))<.00001f);
     pdFree(&f);
    }
    modernParticleFieldsReset();modernParticleFieldsUpdate();CHECK(!initialized);
    {Asteroid particles[4];Node nodes[4];FILE *saved;char line[512];unsigned calls;
     memset(particles,0,sizeof(particles));memset(nodes,0,sizeof(nodes));
     for(i=0;i<4;i++){
        particles[i].objtype=OBJ_AsteroidType;particles[i].asteroidtype=i==3?Asteroid1:Asteroid0;
        particles[i].posinfo.position.x=i==2?100000.0f:4000.0f+2000*i;
        particles[i].posinfo.position.y=particles[i].posinfo.position.z=4000;
        nodes[i].structptr=&particles[i];nodes[i].next=i<3?&nodes[i+1]:NULL;
     }
     universe.MinorSpaceObjList.head=nodes;universe.MinorSpaceObjList.num=4;
     modernParticleFieldsUpdate();CHECK(pointCount==3);CHECK(fieldCount==2);CHECK(submittedCount>2);
     for(i=0;i<(int)submittedCount;i++)CHECK(submitted[i].shape==HW_MODERN_DUST_PARTICLE_DENSITY);
     calls=submissions;modernParticleFieldsUpdate();CHECK(submissions==calls); /* no per-frame rebuild */
     modernParticleFieldsStyle(1,1)->density=.77f;modernParticleFieldsDirty();modernParticleFieldsUpdate();
     calls=submissions;modernParticleFieldsSetOriginalVisible(0);modernParticleFieldsUpdate();
     CHECK(!modernParticleFieldsOriginalVisible());CHECK(submissions==calls);CHECK(submittedCount>0);
     {float tint[3]={.1f,.3f,.8f};modernParticleFieldsSetOriginalColor(tint);}
     modernParticleFieldsSetOriginalOpacity(.25f);
     {float tint[3]={.2f,.4f,.6f};modernOriginalRopeSetColor(tint);}
     modernOriginalRopeSetOpacity(.125f);modernParticleFieldsUpdate();CHECK(submissions==calls);
     modernParticleFieldsStyle(1,1)->radiusScale=.0001234f;
     saved=tmpfile();CHECK(saved);modernParticleFieldsWrite(saved);rewind(saved);modernParticleFieldsResetStyles();modernOriginalRopeReset();
     {float tint[3];CHECK(!modernOriginalRopeColor(tint));CHECK(modernOriginalRopeOpacity()==1);}
     CHECK(modernParticleFieldsOriginalVisible());
     CHECK(fabsf(modernParticleFieldsOriginalOpacity()-168.0f/255.0f)<.000001f);
     while(fgets(line,sizeof(line),saved))modernParticleFieldsRead(line);fclose(saved);
     {float tint[3];CHECK(modernOriginalRopeColor(tint));CHECK(tint[0]==.2f&&tint[2]==.6f);CHECK(modernOriginalRopeOpacity()==.125f);}
     modernParticleFieldsRead("ORIGINAL_ROPE_STYLE 1 nan 0 0 1");
     modernParticleFieldsRead("ORIGINAL_ROPE_STYLE 1 1 1 1 2");
     CHECK(modernOriginalRopeOpacity()==.125f);
     CHECK(modernParticleFieldsStyle(1,0)->density==.77f);
     CHECK(modernParticleFieldsStyle(1,0)->radiusScale==.0001234f);
     CHECK(!modernParticleFieldsOriginalVisible());
     CHECK(fabsf(modernParticleFieldsOriginalOpacity()-.25f)<.000001f);
     modernParticleFieldsRead("PARTICLE_ORIGINAL_OPACITY nan");CHECK(modernParticleFieldsOriginalOpacity()==.25f);
     modernParticleFieldsRead("PARTICLE_ORIGINAL_OPACITY 2");CHECK(modernParticleFieldsOriginalOpacity()==.25f);
     {float tint[3];CHECK(modernParticleFieldsOriginalColor(tint));CHECK(fabsf(tint[0]-.1f)<.00001f&&fabsf(tint[2]-.8f)<.00001f);}
     modernParticleFieldsRead("PARTICLE_DENSITY_STYLE 1 1 nan 1 1 1 1 1 1 1 1 1 1");
     CHECK(modernParticleFieldsStyle(1,0)->density==.77f);
     modernParticleFieldsStyle(-1,1)->enabled=0;modernParticleFieldsDirty();modernParticleFieldsUpdate();CHECK(!submittedCount);
     universe.MinorSpaceObjList.head=NULL;universe.MinorSpaceObjList.num=0;modernParticleFieldsReset();
    }
    free(submittedDensity);
    puts("Actual-particle clustering, additive density, proximity falloff, empty gaps, seamless bricks, save/load, minor-list integration and dirty caching passed.");
    return 0;
}
