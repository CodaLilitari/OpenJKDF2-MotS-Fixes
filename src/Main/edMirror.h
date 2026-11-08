#ifndef ED_MIRROR_H
#define ED_MIRROR_H
#include "edMap.h"
static EdVec edMirrorPoint(EdVec p,int mask){return edV(mask&1?-p.x:p.x,mask&2?-p.y:p.y,mask&4?-p.z:p.z);}
static EdCell edMirrorCell(const EdCell &source,int mask){
    EdCell c=source;c.thrust=edMirrorPoint(c.thrust,mask);int reverse=((mask&1)!=0)^((mask&2)!=0)^((mask&4)!=0);
    for(int f=0;f<c.count;f++){EdFace &face=c.f[f];face.attributes=edSurfaceAttributes(&source.f[f]);face.hasAttributes=1;face.neighbor=face.mirror=-1;
        for(int v=0;v<face.count;v++)face.v[v].p=edMirrorPoint(face.v[v].p,mask);
        if(reverse)for(int v=0;v<face.count/2;v++){EdVertex temp=face.v[v];face.v[v]=face.v[face.count-1-v];face.v[face.count-1-v]=temp;}
    }return c;
}
static int edMirrorSameFace(const EdFace &a,const EdFace &b,int attributes){
    if(a.count!=b.count)return 0;
    if(attributes&&(a.material!=b.material||a.uOffset!=b.uOffset||a.vOffset!=b.vOffset||a.uvRotation!=b.uvRotation||a.customUV!=b.customUV))return 0;
    if(attributes){EdSurfaceAttributes x=edSurfaceAttributes(&a),y=edSurfaceAttributes(&b);if(x.surfaceFlags!=y.surfaceFlags||x.faceFlags!=y.faceFlags||x.adjoinFlags!=y.adjoinFlags||x.geometryMode!=y.geometryMode||x.textureMode!=y.textureMode||x.lightingMode!=y.lightingMode||x.portal!=y.portal||x.uScale!=y.uScale||x.vScale!=y.vScale||x.extraLight!=y.extraLight||x.adjoinDistance!=y.adjoinDistance)return 0;}
    for(int v=0;v<a.count;v++){int found=0;for(int w=0;w<b.count;w++){EdVec d=edSub(a.v[v].p,b.v[w].p);if(edDot(d,d)<1e-14&&(!attributes||(fabs(a.v[v].u-b.v[w].u)<1e-8&&fabs(a.v[v].v-b.v[w].v)<1e-8&&fabs(a.v[v].light-b.v[w].light)<1e-8))){found=1;break;}}if(!found)return 0;}return 1;
}
static int edMirrorSameCell(const EdCell &a,const EdCell &b,int attributes){
    if(a.count!=b.count)return 0;if(attributes&&(a.flags!=b.flags||a.ambientLight!=b.ambientLight||a.extraLight!=b.extraLight||strcmp(a.colormap,b.colormap)||strcmp(a.sound,b.sound)||a.soundVolume!=b.soundVolume))return 0;
    EdVec d=edSub(edMapCenter(&a),edMapCenter(&b));if(edDot(d,d)>1e-12)return 0;
    for(int f=0;f<a.count;f++){int found=0;for(int g=0;g<b.count;g++)if(edMirrorSameFace(a.f[f],b.f[g],attributes)){found=1;break;}if(!found)return 0;}return 1;
}
/* Reflect only this transaction's changed geometry, never the whole level.
   Existing counterparts are replaced/removed; original edits win conflicts. */
static int edMirrorEdit(EdModel *out,const EdModel *before,const EdModel *after,int axes){
    if(!axes){edCopyModel(out,after);return 1;}EdBuffer<unsigned char> changed,removed,erase;int changes=0;
    for(int s=0;s<after->count;s++){int same=0;for(int b=0;b<before->count;b++)if(edMirrorSameCell(after->c[s],before->c[b],1)){same=1;break;}changed[s]=!same;changes+=!same;}
    for(int b=0;b<before->count;b++){int same=0;for(int s=0;s<after->count;s++)if(edMirrorSameCell(before->c[b],after->c[s],1)){same=1;break;}removed[b]=!same;changes+=!same;}
    if(!changes){edCopyModel(out,after);return 1;}EdModel draft;edCopyModel(&draft,after);
    int eraseCount=0;
    for(int mask=1;mask<8;mask++)if((mask&axes)==mask)for(int b=0;b<before->count;b++)if(removed[b]){EdCell reflected=edMirrorCell(before->c[b],mask);for(int s=0;s<after->count;s++)if(!changed[s]&&!erase[s]&&edMirrorSameCell(reflected,after->c[s],0)){erase[s]=1;eraseCount++;}}
    // Remove counterparts before adding replacements: resolving overlap while
    // the old cell still exists would discard the new cleaved halves.
    if(eraseCount&&!edMapDelete(&draft,after,erase,1))return 0;
    for(int mask=1;mask<8;mask++)if((mask&axes)==mask)for(int s=0;s<after->count;s++)if(changed[s]){EdCell reflected=edMirrorCell(after->c[s],mask);int exists=0;for(int d=0;d<draft.count;d++)if(edMirrorSameCell(reflected,draft.c[d],0)){exists=1;break;}
        if(!exists){if(draft.count>=ED_CELLS)return 0;draft.c[draft.count++]=std::move(reflected);}
    }
    for(int i=0;i<draft.splineCount;i++)for(int s=0;s<after->count;s++)if(changed[s]&&after->c[s].splineId==draft.splines[i].id){draft.splines[i].mirrorAxes|=axes;break;}
    if(!edResolve(&draft)||!edValidate(&draft))return 0;edCopyModel(out,&draft);return 1;
}
#endif
