/* Standard 3DO 2.1 output plus versioned editable metadata. */
#include "edWire.h"
int edReadMeshData(EdModel *model,const unsigned char *bytes,unsigned int size){
    if(size<8||edCompare(bytes,"G3DO",4)||(edNetLoad32(bytes+4)<1||edNetLoad32(bytes+4)>3))return 0;
    EdModel *draft=edCalloc(1,sizeof(*draft));if(!draft)return 0;EdWire w={(unsigned char*)bytes,size,8,1,1};if(edNetLoad32(bytes+4)>=3)edWireAssets(&w,draft);else{edWireAssetsLegacy(&w,draft);for(int a=0;w.ok&&a<draft->assetCount;a++)draft->assets[a].saved=1;}if(edNetLoad32(bytes+4)>=2)edWireTemplates(&w,draft);
    int ok=w.ok&&w.at==size&&edMeshValidate(draft);if(ok){model->templateCount=draft->templateCount;edCopy(model->templates,draft->templates,sizeof(model->templates));model->assetCount=draft->assetCount;model->meshCount=draft->meshCount;edCopy(model->assets,draft->assets,sizeof(model->assets));edCopy(model->meshes,draft->meshes,sizeof(model->meshes));}edFree(draft);return ok;
}
static int meshMaterialName(const char *resources,int id,char name[128]){
    const char *p=strstr(resources,"World materials");if(!p)p=strstr(resources,"world materials");if(!p)return 0;
    p=strchr(p,'\n');if(!p)return 0;p++;
    while(*p&&strncmp(p,"end",3)){int index;char value[128];if(sscanf(p," %d: %127s",&index,value)==2&&index==id){strcpy(name,value);return 1;}p=strchr(p,'\n');if(!p)break;p++;}return 0;
}
static int mesh3do(FILE *out,const EdModel *m,int asset,const char *resources){
    if(asset<0||asset>=m->assetCount||!edMeshValidate(m))return 0;
    EdBuffer<int> ids;int materials=0,meshes=0;double radius=.001;
    for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset){meshes++;const EdCell *c=&m->meshes[i].geometry;
        for(int f=0;f<c->count;f++){int id=c->f[f].material,j;for(j=0;j<materials&&ids[j]!=id;j++);if(j==materials)ids[materials++]=id;
            for(int v=0;v<c->f[f].count;v++){EdVec p=edSub(c->f[f].v[v].p,m->assets[asset].origin);radius=fmax(radius,sqrt(edDot(p,p)));}}}
    if(!meshes)return 0;
    fprintf(out,"SECTION: HEADER\n3DO 2.1\nSECTION: MODELRESOURCE\nMATERIALS %d\n",materials);
    for(int i=0;i<materials;i++){char name[128];if(!meshMaterialName(resources,ids[i],name))return 0;fprintf(out,"%d: %s\n",i,name);}
    fprintf(out,"SECTION: GEOMETRYDEF\nRADIUS %.9f\nINSERT OFFSET 0 0 0\nGEOSETS 1\nGEOSET 0\nMESHES %d\n",radius,meshes);
    int index=0;for(int i=0;i<m->meshCount;i++)if(m->meshes[i].asset==asset){const EdCell *c=&m->meshes[i].geometry;int count=0;for(int f=0;f<c->count;f++)count+=c->f[f].count;
        fprintf(out,"MESH %d\nNAME mesh%d\nRADIUS %.9f\nGEOMETRYMODE 4\nLIGHTINGMODE 3\nTEXTUREMODE 1\nVERTICES %d\n",index,index,radius,count);index++;
        int vnum=0;for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++){EdVec p=edSub(c->f[f].v[v].p,m->assets[asset].origin);fprintf(out,"%d: %.9f %.9f %.9f 0\n",vnum++,p.x,p.y,p.z);}
        fprintf(out,"TEXTURE VERTICES %d\n",count);vnum=0;for(int f=0;f<c->count;f++)for(int v=0;v<c->f[f].count;v++)fprintf(out,"%d: %.9f %.9f\n",vnum++,c->f[f].v[v].u,c->f[f].v[v].v);
        fputs("VERTEX NORMALS\n",out);vnum=0;for(int f=0;f<c->count;f++){EdVec n=edNormal(&c->f[f]);for(int v=0;v<c->f[f].count;v++)fprintf(out,"%d: %.9f %.9f %.9f\n",vnum++,n.x,n.y,n.z);}
        fprintf(out,"FACES %d\n",c->count);vnum=0;for(int f=0;f<c->count;f++){const EdFace *face=&c->f[f];int mat=0;while(mat<materials&&ids[mat]!=face->material)mat++;fprintf(out,"%d: %d 0x0 4 3 1 0 %d",f,mat,face->count);for(int v=0;v<face->count;v++,vnum++)fprintf(out," %d,%d",vnum,vnum);fputc('\n',out);}
        fputs("FACE NORMALS\n",out);for(int f=0;f<c->count;f++){EdVec n=edNormal(&c->f[f]);fprintf(out,"%d: %.9f %.9f %.9f\n",f,n.x,n.y,n.z);}
    }
    fprintf(out,"SECTION: HIERARCHYDEF\nHIERARCHY NODES %d\n",meshes);
    for(int i=0;i<meshes;i++)fprintf(out,"%d: 0x0 0x0 %d %d %d %d %d 0 0 0 0 0 0 0 0 0 mesh%d\n",i,i,i?0:-1,i==0&&meshes>1?1:-1,i>0&&i+1<meshes?i+1:-1,i==0?meshes-1:0,i);
    return !ferror(out);
}
int edWrite3do(const EdModel *m,int asset,const char *path,const char *resources){FILE *f=fopen(path,"wb");if(!f)return 0;int ok=mesh3do(f,m,asset,resources);if(fclose(f))ok=0;if(!ok)remove(path);return ok;}
static int writeMeshAssets(const EdModel *m,const char *path,const char *resources){
    if(!m->assetCount&&!m->templateCount)return 1;EdWire measure={NULL,ED_NET_LIMIT/2,8,0,1};edWireAssets(&measure,(EdModel*)m);edWireTemplates(&measure,(EdModel*)m);if(!measure.ok)return 0;unsigned char *bytes=edAlloc(measure.at);if(!bytes)return 0;edCopy(bytes,"G3DO",4);edNetStore32(bytes+4,3);EdWire w={bytes,measure.at,8,0,1};edWireAssets(&w,(EdModel*)m);edWireTemplates(&w,(EdModel*)m);
    int ok=w.ok&&edGobAddFile(path,"misc\\geosith_models.bin",bytes,(unsigned int)w.at);edFree(bytes);
    for(int a=0;a<m->assetCount&&ok;a++){if(!m->assets[a].saved)continue;int count=0;for(int i=0;i<m->meshCount;i++)count+=m->meshes[i].asset==a;if(!count)continue;
        FILE *f=tmpfile();if(!f)return 0;ok=mesh3do(f,m,a,resources);long size=ftell(f);unsigned char *data=size>0?(unsigned char*)edAlloc(size):NULL;
        if(ok&&data&&!fseek(f,0,SEEK_SET)&&fread(data,1,size,f)==size){char name[128];snprintf(name,sizeof(name),"3do\\%s.3do",m->assets[a].name);ok=edGobAddFile(path,name,data,(unsigned int)size);}else ok=0;edFree(data);fclose(f);
    }return ok;
}
