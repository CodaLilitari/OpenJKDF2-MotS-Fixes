/* Software thumbnails work in both the classic menu buffer and the game HUD.
 * Their depth buffer and palette are private: drawing a thumbnail never changes
 * the world renderer's camera, material state, or joint animation matrices. */
typedef struct {double x,y,z,u,v;} EditorPreviewVertex;
typedef struct {rdMaterial *material;tVBuffer *target,*source;rdTexinfo *info;int lut[256];} EditorPreviewTexture;
typedef struct {int id,width,height,bpp;unsigned int paletteHash;unsigned int age;uint32_t *pixels;} EditorPreviewCache;
static EditorPreviewCache previewCache[64];
static unsigned int previewAge;
static void clearObjectPreviews(void) {
    for(int i=0;i<64;i++)edFree(previewCache[i].pixels);edZero(previewCache,0,sizeof(previewCache));
}
static void previewNode(rdModel3 *model,rdHierarchyNode *node,const rdMatrix34 *parent,rdMatrix34 *meshes,int depth) {
    if(!node||depth>model->numHNodes)return;
    rdMatrix34 local,pivot,result;rdMatrix_Build34(&local,&node->rot,&node->pos);
    rdMatrix_BuildTranslate34(&pivot,&node->pivot);rdMatrix_PostMultiply34(&pivot,&local);
    if(node->parent){rdVector3 negative;rdVector_Neg3(&negative,&node->parent->pivot);rdMatrix_PostTranslate34(&pivot,&negative);}
    rdMatrix_Multiply34(&result,parent,&pivot);
    if(node->meshIdx>=0&&(unsigned int)node->meshIdx<model->aGeos[0].numMeshes)meshes[node->meshIdx]=result;
    rdHierarchyNode *child=node->child;for(int i=0;child&&i<node->numChildren;i++,child=child->nextSibling)previewNode(model,child,&result,meshes,depth+1);
}
static rdMatrix34 *previewMatrices(rdModel3 *model) {
    if(!model||!model->numGeos)return NULL;
    rdMatrix34 *matrices=edCalloc(model->aGeos[0].numMeshes,sizeof(*matrices));if(!matrices)return NULL;
    rdMatrix34 identity;rdMatrix_Identity34(&identity);
    for(unsigned int i=0;i<model->aGeos[0].numMeshes;i++)matrices[i]=identity;
    for(int i=0;i<model->numHNodes;i++)if(!model->aHierarchyNodes[i].parent)previewNode(model,&model->aHierarchyNodes[i],&identity,matrices,0);
    return matrices;
}
static EditorPreviewVertex previewProject(rdVector3 p) {
    return EditorPreviewVertex{.8*p.x-.6*p.y,.3*p.x+.4*p.y-.8660254*p.z,.5196152*p.x+.6928203*p.y+.5*p.z,0,0};
}
static double previewArea(EditorPreviewVertex a,EditorPreviewVertex b,double x,double y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static int previewSample(EditorPreviewTexture *texture,double u,double v) {
    if(!texture)return -1;
    tVBuffer *source=texture->source;
    if(!source)return texture->lut[texture->info->header.solidColor&255];
    int sx=(int)floor(u)%source->format.width,sy=(int)floor(v)%source->format.height;if(sx<0)sx+=source->format.width;if(sy<0)sy+=source->format.height;
    unsigned char *p=(unsigned char*)source->surface_lock_alloc+sy*source->format.rowSize+sx*(source->format.format.bpp/8);
    if(source->format.format.bpp==8){if(texture->info->texture_ptr->alpha_en&&*p==texture->info->texture_ptr->color_transparent)return -1;return texture->lut[*p];}
    if(source->format.format.bpp==16){
        uint16_t value;edCopy(&value,p,2);rdTexFormat *fmt=&source->format.format;
        int r=fmt->r_bits?((value>>fmt->r_shift)&((1u<<fmt->r_bits)-1))*255/((1u<<fmt->r_bits)-1):0;
        int g=fmt->g_bits?((value>>fmt->g_shift)&((1u<<fmt->g_bits)-1))*255/((1u<<fmt->g_bits)-1):0;
        int b=fmt->b_bits?((value>>fmt->b_shift)&((1u<<fmt->b_bits)-1))*255/((1u<<fmt->b_bits)-1):0;
        return palettePixel(texture->target,r,g,b);
    }
    return -1;
}
static void previewTriangle(EditorPreviewVertex a,EditorPreviewVertex b,EditorPreviewVertex c,EditorPreviewTexture *texture,int fallback,int width,int height,uint32_t *pixels,double *depth) {
    double area=previewArea(a,b,c.x,c.y);if(fabs(area)<.00001)return;
    int x0=(int)fmax(0,floor(fmin(a.x,fmin(b.x,c.x)))),x1=(int)fmin(width-1,ceil(fmax(a.x,fmax(b.x,c.x))));
    int y0=(int)fmax(0,floor(fmin(a.y,fmin(b.y,c.y)))),y1=(int)fmin(height-1,ceil(fmax(a.y,fmax(b.y,c.y))));
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
        double w0=previewArea(b,c,x+.5,y+.5)/area,w1=previewArea(c,a,x+.5,y+.5)/area,w2=1-w0-w1;
        if(w0<-.000001||w1<-.000001||w2<-.000001)continue;
        double z=w0*a.z+w1*b.z+w2*c.z;int index=y*width+x;if(z<=depth[index])continue;
        int color=texture?previewSample(texture,w0*a.u+w1*b.u+w2*c.u,w0*a.v+w1*b.v+w2*c.v):fallback;
        if(color<0)continue;pixels[index]=(uint32_t)color;depth[index]=z;
    }
}
static EditorPreviewCache *renderObjectPreview(tVBuffer *buffer,rdModel3 *model,int id,int width,int height) {
    unsigned int hash=2166136261u;unsigned char *colors=(unsigned char*)stdDisplay_masterPalette;
    for(int i=0;i<768;i++)hash=(hash^colors[i])*16777619u;
    for(int i=0;liveColormap[i];i++)hash=(hash^(unsigned char)liveColormap[i])*16777619u;
    int slot=0;
    for(int i=0;i<64;i++){
        EditorPreviewCache *cache=&previewCache[i];
        if(cache->pixels&&cache->id==id&&cache->width==width&&cache->height==height&&cache->bpp==buffer->format.format.bpp&&cache->paletteHash==hash){cache->age=++previewAge;return cache;}
        if(!cache->pixels||cache->age<previewCache[slot].age)slot=i;
    }
    rdMatrix34 *matrices=previewMatrices(model);if(!matrices)return NULL;
    EditorPreviewCache *cache=&previewCache[slot];edFree(cache->pixels);edZero(cache,0,sizeof(*cache));
    cache->pixels=edAlloc(width*height*sizeof(*cache->pixels));double *depth=edAlloc(width*height*sizeof(*depth));
    if(!cache->pixels||!depth){edFree(cache->pixels);cache->pixels=NULL;edFree(depth);edFree(matrices);return NULL;}
    cache->id=id;cache->width=width;cache->height=height;cache->bpp=buffer->format.format.bpp;cache->paletteHash=hash;cache->age=++previewAge;
    for(int i=0;i<width*height;i++){cache->pixels[i]=0x10000;depth[i]=-1e30;}
    rdGeoset *geo=&model->aGeos[0];double loX=1e30,loY=1e30,hiX=-1e30,hiY=-1e30;
    for(unsigned int m=0;m<geo->numMeshes;m++)for(int v=0;v<geo->aMeshes[m].numVertices;v++){
        rdVector3 p;rdMatrix_TransformPoint34(&p,&geo->aMeshes[m].aVertices[v],&matrices[m]);EditorPreviewVertex q=previewProject(p);
        loX=fmin(loX,q.x);hiX=fmax(hiX,q.x);loY=fmin(loY,q.y);hiY=fmax(hiY,q.y);
    }
    double scale=fmin((width-6)/fmax(.00001,hiX-loX),(height-6)/fmax(.00001,hiY-loY));
    EditorPreviewTexture textures[64];int textureCount=0;edZero(textures,0,sizeof(textures));int fallback=palettePixel(buffer,150,190,210);
    for(unsigned int m=0;m<geo->numMeshes;m++){
        rdMesh *mesh=&geo->aMeshes[m];
        for(int f=0;f<mesh->numFaces;f++){
            rdFace *face=&mesh->faces[f];if(face->numVertices<3||!face->geometryMode)continue;
            EditorPreviewTexture *texture=NULL;
            if(face->material){for(int i=0;i<textureCount;i++)if(textures[i].material==face->material)texture=&textures[i];
                if(!texture&&textureCount<64){texture=&textures[textureCount++];texture->material=face->material;texture->target=buffer;rdMaterial *mat=face->material;
                    if(rdMaterial_EnsureDataForced(mat)&&mat->num_texinfo&&mat->texinfos[0]){
                        texture->info=mat->texinfos[0];texture->source=texture->info->texture_ptr?texture->info->texture_ptr->texture_struct[0]:NULL;
                        rdColor24 *paletteColors=mat->palette_alloc?mat->palette_alloc:colormapColors(addColormap(liveColormap));
                        for(int i=0;i<paletteCount;i++)if(!mat->palette_alloc&&!__strcmpi(baseName(mat->mat_fpath),paletteNames[i])){paletteColors=colormapColors(materialColormap[i]);break;}
                        for(int i=0;i<256;i++)texture->lut[i]=palettePixel(buffer,paletteColors[i].r,paletteColors[i].g,paletteColors[i].b);
                        if(texture->source&&(!stdDisplay_VBufferLock(texture->source)||!texture->source->surface_lock_alloc||texture->source->format.width<1||texture->source->format.height<1)){stdDisplay_VBufferUnlock(texture->source);texture->source=NULL;}
                    }
                }
            }
            if(texture&&!texture->info)texture=NULL;
            EditorPreviewVertex vertices[64];if(face->numVertices>64)continue;int valid=1;
            for(unsigned int v=0;v<face->numVertices;v++){
                int index=face->vertexPosIdx[v];if(index<0||index>=mesh->numVertices){valid=0;break;}
                rdVector3 p;rdMatrix_TransformPoint34(&p,&mesh->aVertices[index],&matrices[m]);vertices[v]=previewProject(p);
                vertices[v].x=width*.5+(vertices[v].x-(loX+hiX)*.5)*scale;vertices[v].y=height*.5+(vertices[v].y-(loY+hiY)*.5)*scale;
                int uv=face->vertexUVIdx?face->vertexUVIdx[v]:-1;
                if(uv>=0&&uv<mesh->numUVs){vertices[v].u=mesh->aTexVerticies[uv].x+face->texVertOffset.x;vertices[v].v=mesh->aTexVerticies[uv].y+face->texVertOffset.y;}
            }
            if(valid)for(unsigned int v=1;v+1<face->numVertices;v++)previewTriangle(vertices[0],vertices[v],vertices[v+1],texture,fallback,width,height,cache->pixels,depth);
        }
    }
    for(int i=0;i<textureCount;i++)if(textures[i].source)stdDisplay_VBufferUnlock(textures[i].source);
    edFree(depth);edFree(matrices);return cache;
}
