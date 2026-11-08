/* Included after the convex geometry implementation. */
EdVec edSplinePosition(const EdSpline *s,double t){
    if(s->count<2)return s->count?s->points[0].position:edV(0,0,0);
    int spans=s->closed?s->count:s->count-1;t=fmax(0,fmin(spans,t));int i=(int)t;if(i>=spans)i=spans-1;t-=i;double u=1-t;
    const EdSplinePoint &a=s->points[i],&b=s->points[(i+1)%s->count];
    return edAdd(edAdd(edScale(a.position,u*u*u),edScale(edAdd(a.position,a.handle),3*u*u*t)),edAdd(edScale(edSub(b.position,b.handle),3*u*t*t),edScale(b.position,t*t*t)));
}
static unsigned splineHash(const EdModel *m,int id){
    unsigned hash=2166136261u;for(int i=0;i<m->count;i++)if(m->c[i].splineId==id){EdCell c=m->c[i];
        for(int f=0;f<c.count;f++){c.f[f].neighbor=c.f[f].neighbor<0?-1:0;c.f[f].mirror=-1;}
        hash=(hash^edCellHash(&c))*16777619u;
    }return hash;
}
int edSplineValidate(const EdModel *m){
    if(m->splineCount<0||m->splineCount>ED_SPLINES)return 0;
    for(int i=0;i<m->splineCount;i++){const EdSpline &s=m->splines[i];
        if(s.mirrorAxes<0||s.mirrorAxes>7||(s.closed!=0&&s.closed!=1)||(s.closed&&s.count<3)||s.id<=0||s.count<1||s.count>ED_SPLINE_POINTS||s.sides<3||s.sides>32||s.segments<1||s.segments>ED_SPLINE_SECTORS||s.material<0||!isfinite(s.width)||!isfinite(s.height)||s.width<ED_GRID||s.height<ED_GRID||s.width>1000||s.height>1000)return 0;
        for(int j=0;j<i;j++)if(m->splines[j].id==s.id)return 0;
        for(int j=0;j<s.count;j++){const EdSplinePoint &p=s.points[j];double a[]={p.position.x,p.position.y,p.position.z,p.handle.x,p.handle.y,p.handle.z};for(double v:a)if(!isfinite(v)||fabs(v)>100000)return 0;}
    }return 1;
}
void edSplineReconcile(EdModel *m){
    // CSG intersections and adjoin changes are not a request to bake controls.
    for(int i=0;i<m->splineCount;i++)m->splines[i].geometryHash=splineHash(m,m->splines[i].id);
    for(int c=0;c<m->count;c++)if(m->c[c].splineId){int found=0;for(int i=0;i<m->splineCount;i++)found|=m->splines[i].id==m->c[c].splineId;if(!found)m->c[c].splineId=0;}
}
int edSplineConvert(EdModel *m,int id){
    for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id){
        for(int c=0;c<m->count;c++)if(m->c[c].splineId==id)m->c[c].splineId=0;
        for(int j=i+1;j<m->splineCount;j++)m->splines[j-1]=std::move(m->splines[j]);m->splineCount--;return 1;
    }return 0;
}
int edSplineRebuild(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight){
    if(textureHeight<1||textureHeight>16384)return 0;
    EdModel work;edCopyModel(&work,source);EdSpline settings=*spline;int index=-1;
    for(int i=0;i<work.splineCount;i++)if(work.splines[i].id==settings.id)index=i;
    if(index<0){if(work.splineCount==ED_SPLINES)return 0;index=work.splineCount++;}
    work.splines[index]=settings;if(!edSplineValidate(&work))return 0;
    EdBuffer<int> remap;int count=0;
    for(int i=0;i<work.count;i++){remap[i]=work.c[i].splineId==settings.id?-1:count;if(remap[i]>=0)work.c[count++]=work.c[i];}
    for(int c=0;c<work.cogCount;c++)for(int p=0;p<work.cogs[c].count;p++){EdCogParam &v=work.cogs[c].params[p];if((v.type==ED_COG_SECTOR||v.type==ED_COG_SURFACE)&&v.cell>=0&&v.cell<work.count){v.cell=remap[v.cell];if(v.cell<0)v.face=-1;}}
    work.count=count;work.c.resize(count);
    if(settings.count>=2){
        /* Arc-length sampling makes the requested number of sectors useful even
           when control points are unevenly spaced. Shared upright miter planes
           keep adjoining rings identical while allowing ramps via point Z. */
        int spans=settings.closed?settings.count:settings.count-1;int samples=spans*64;std::vector<double> distances(samples+1);EdVec previous=edSplinePosition(&settings,0);
        for(int i=1;i<=samples;i++){EdVec p=edSplinePosition(&settings,spans*(double)i/samples);distances[i]=distances[i-1]+length(edSub(p,previous));previous=p;}
        if(distances.back()<ED_GRID)return 0;
        if(settings.closed&&settings.segments<3)return 0;
        std::vector<EdVec> centers(settings.segments+1),rights(settings.segments+1);
        for(int i=0;i<=settings.segments;i++){double d=distances.back()*i/settings.segments;int k=(int)(std::lower_bound(distances.begin(),distances.end(),d)-distances.begin());if(k<1)k=1;
            double u=distances[k]>distances[k-1]?(d-distances[k-1])/(distances[k]-distances[k-1]):0,t=spans*(k-1+u)/samples;centers[i]=edSplinePosition(&settings,t);
        }
        for(int i=0;i<=settings.segments;i++){EdVec tangent=settings.closed?edSub(centers[(i+1)%settings.segments],centers[(i+settings.segments-1)%settings.segments]):edSub(centers[i==settings.segments?i:i+1],centers[i?i-1:i]);tangent.z=0;if(length(tangent)<ED_EPS)return 0;rights[i]=unit(edCross(tangent,edV(0,0,1)));}
        std::vector<EdVec> ring((settings.segments+1)*settings.sides);
        for(int i=0;i<=settings.segments;i++)for(int v=0;v<settings.sides;v++){double a=2*3.141592653589793*v/settings.sides+(settings.sides==4?3.141592653589793/4:3.141592653589793/2),scale=settings.sides==4?sqrt(2.0):1;
            ring[i*settings.sides+v]=edAdd(centers[i],edAdd(edScale(rights[i],cos(a)*settings.width*.5*scale),edV(0,0,sin(a)*settings.height*.5*scale)));
        }
        std::vector<double> perimeter(settings.sides+1);
        for(int v=0;v<settings.sides;v++)perimeter[v+1]=perimeter[v]+length(edSub(ring[(v+1)%settings.sides],ring[v]));
        double wrap=fmax(1,round(perimeter.back()*ED_UV_SCALE/textureHeight))*textureHeight;
        for(int i=0;i<settings.segments;i++){EdCell cell={};cell.extraLight=.5;cell.splineId=settings.id;cell.count=2;
            for(int f=0;f<2;f++){cell.f[f].count=settings.sides;cell.f[f].neighbor=cell.f[f].mirror=-1;cell.f[f].material=settings.material;for(int v=0;v<settings.sides;v++)cell.f[f].v[v].p=ring[(i+f)*settings.sides+v];}
            if(!hull(&cell,1e-8))return 0;
            int faces=cell.count;for(int f=0;f<faces;f++)if(cell.f[f].count>ED_SURFACE_VERTS){EdFace polygon=cell.f[f];cell.f[f].count=3;for(int v=2;v<polygon.count-1;v++){EdFace triangle=polygon;triangle.count=3;triangle.v[0]=polygon.v[0];triangle.v[1]=polygon.v[v];triangle.v[2]=polygon.v[v+1];cell.f[cell.count++]=triangle;}}
            for(int f=0;f<cell.count;f++){EdFace &face=cell.f[f];int rings[ED_VERTS],corners[ED_VERTS],mask=0,first=0,last=0;
                for(int v=0;v<face.count;v++){int found=-1;for(int k=0;k<2*settings.sides;k++)if(same(face.v[v].p,ring[i*settings.sides+k])){found=k;break;}if(found<0)return 0;
                    rings[v]=found/settings.sides;corners[v]=found%settings.sides;mask|=1<<rings[v];first|=corners[v]==0;last|=corners[v]==settings.sides-1;}
                if(mask!=3)continue; // End caps retain their planar mapping.
                face.customUV=1;
                for(int v=0;v<face.count;v++){int corner=corners[v];if(first&&last&&corner==0)corner=settings.sides;
                    face.v[v].u=distances.back()*(i+rings[v])/settings.segments*ED_UV_SCALE;
                    face.v[v].v=perimeter[corner]/perimeter.back()*wrap;}
            }
            work.c[work.count++]=std::move(cell);
        }
    }
    int primaryCount=work.count;
    for(int mask=1;mask<8;mask++)if((mask&settings.mirrorAxes)==mask)for(int c=0;c<primaryCount;c++)if(work.c[c].splineId==settings.id){
        if(work.count>=ED_CELLS)return 0;EdCell reflected=work.c[c];int reverse=((mask&1)!=0)^((mask&2)!=0)^((mask&4)!=0);
        for(int f=0;f<reflected.count;f++){EdFace &face=reflected.f[f];for(int v=0;v<face.count;v++){EdVec &p=face.v[v].p;if(mask&1)p.x=-p.x;if(mask&2)p.y=-p.y;if(mask&4)p.z=-p.z;}
            if(reverse)for(int v=0;v<face.count/2;v++)std::swap(face.v[v],face.v[face.count-1-v]);}
        work.c[work.count++]=std::move(reflected);
    }
    for(int c=0;c<work.count;c++)for(int f=0;f<work.c[c].count;f++){EdFace &face=work.c[c].f[f];face.attributes=edSurfaceAttributes(&face);face.hasAttributes=1;face.neighbor=face.mirror=-1;}
    if(!edResolve(&work))return 0;

    work.splines[index].geometryHash=splineHash(&work,settings.id);edSplineReconcile(&work);
    if(!edValidate(&work))return 0;edCopyModel(out,&work);return 1;
}
void edSplineReadMetadata(EdModel *m,const char *text){
    m->splineCount=0;for(int c=0;c<m->count;c++)m->c[c].splineId=0;
    for(const char *line=text;line&&*line;line=strchr(line,'\n')){if(*line=='\n')line++;EdSpline s={};int id,point,cell;EdSplinePoint p={};
        if(sscanf(line,"# GeoSith spline %d %d %d %d %lf %lf",&s.id,&s.sides,&s.segments,&s.material,&s.width,&s.height)==6&&m->splineCount<ED_SPLINES)m->splines[m->splineCount++]=s;
        int axes;if(sscanf(line,"# GeoSith splinemirror %d %d",&id,&axes)==2)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id)m->splines[i].mirrorAxes=axes;
        if(sscanf(line,"# GeoSith splineclosed %d",&id)==1)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id)m->splines[i].closed=1;
        if(sscanf(line,"# GeoSith splinepoint %d %d %lf %lf %lf %lf %lf %lf",&id,&point,&p.position.x,&p.position.y,&p.position.z,&p.handle.x,&p.handle.y,&p.handle.z)==8)for(int i=0;i<m->splineCount;i++)if(m->splines[i].id==id&&point==m->splines[i].count&&point<ED_SPLINE_POINTS)m->splines[i].points[m->splines[i].count++]=p;
        if(sscanf(line,"# GeoSith splinecell %d %d",&cell,&id)==2&&cell>=0&&cell<m->count)m->c[cell].splineId=id;
    }
    if(!edSplineValidate(m)){m->splineCount=0;for(int c=0;c<m->count;c++)m->c[c].splineId=0;return;}
    for(int i=0;i<m->splineCount;i++)m->splines[i].geometryHash=splineHash(m,m->splines[i].id);
}
