static int terrainSelected=-1,terrainStroke;
static double terrainRadius=.5,terrainElapsed;
static EdHit terrainHover={-1,-1,-1,-1};
static int terrainPick(EdVec origin,EdVec direction,EdHit *hit){
    *hit=EdHit{-1,-1,-1,-1,1e30};EdHit wall;edRay(model,origin,direction,&wall);
    for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain){EdHit h;if(edMeshRay(model,a,origin,direction,0,&h)&&h.distance<hit->distance&&(wall.cell<0||h.distance<=wall.distance+.002))*hit=h;}
    return hit->cell>=0?model->meshes[hit->cell].asset:-1;
}
static int terrainCreate(void){
    if(netEditPending())return 0;EdSelection surfaces=picks;
    if(!surfaces.count){if(hover.cell<0)return 0;surfaces.count=1;surfaces.hits[0]=hover;surfaces.hits[0].edge=surfaces.hits[0].vertex=-1;}
    char name[32];int found=0;
    for(int i=1;i<10000;i++){snprintf(name,32,"terrain%d",i);found=edTemplateIndex(model,name)>=0||sithTemplate_GetTemplate(name)!=NULL;for(int a=0;a<model->assetCount;a++)found|=!strcmp(name,model->assets[a].name);char file[64];snprintf(file,64,"%s.3do",name);found|=builderNameExists(file);if(!found)break;}
    if(found||!edTerrainCreate(candidate,model,&surfaces,name)){say("Terrain needs solid surfaces and free model/mesh capacity.");return 0;}
    int asset=model->assetCount;for(int a=asset;a<candidate->assetCount;a++)if(!builderWalkstruct(candidate,a,candidate->assets[a].name))return 0;
    remember();if(!commit())return 0;terrainSelected=asset;picks.count=0;sectorPicks.clear();
    say("Terrain created. Fire 1 raises; Shift lowers; Ctrl smooths; wheel resizes. Edges can be sculpted too. Middle mouse: terrain options.");return 1;
}
static int terrainConvert(int asset,int walkstruct,int remove=0){
    if(asset<0||asset>=model->assetCount||!model->assets[asset].terrain||netEditPending())return 0;
    edCopyModel(candidate,model);int group=model->assets[asset].terrainGroup;
    for(int part=model->assetCount-1;part>=0;part--){if(part!=asset&&(!group||model->assets[part].terrainGroup!=group||!model->assets[part].terrain))continue;
    char name[32];snprintf(name,32,"%s",candidate->assets[part].name);
    if(!walkstruct){
        for(int o=candidate->objectCount-1;o>=0;o--)if(!strcmp(candidate->objects[o].templateName,name)){for(int j=o+1;j<candidate->objectCount;j++)candidate->objects[j-1]=candidate->objects[j];candidate->objectCount--;edCogThingChanged(candidate,candidate->spawnCount+o,0);}
        int t=edTemplateIndex(candidate,name);if(t>=0){for(int j=t+1;j<candidate->templateCount;j++)candidate->templates[j-1]=candidate->templates[j];candidate->templateCount--;}
    }
    candidate->assets[part].terrain=0;candidate->assets[part].terrainGroup=0;candidate->assets[part].terrainNormal=edV(0,0,0);for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==part){candidate->meshes[i].terrainPinned.clear();candidate->meshes[i].terrainNormals.clear();}
    if(remove)builderRemoveAsset(candidate,part);
    }
    remember();if(!commit())return 0;terrainSelected=-1;if(remove){say("Terrain deleted. Ctrl+Z: undo.");return 1;}say(walkstruct?"Terrain converted to a normal walkstruct in place. Ctrl+Z restores sculpting.":"Terrain saved in the custom 3DO library and removed from the scene. Ctrl+Z restores it.");return 1;
}
static int terrainDelete(int asset){terrainStroke=grabbing=0;terrainHover.cell=-1;return terrainConvert(asset,0,1);}
static void terrainMenu(void){
    BuilderMenuState state=builderMenuBegin();
    if(terrainSelected<0||terrainSelected>=model->assetCount||!model->assets[terrainSelected].terrain){char16_t names[ED_ASSETS][128];int ids[ED_ASSETS],count=0;for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain){ids[count]=a;stdString_CharToWchar(names[count++],model->assets[a].name,128);}int choice=chooseList(u"Choose Terrain",names,count,0);terrainSelected=choice>=0?ids[choice]:-1;}
    if(terrainSelected>=0&&terrainSelected<model->assetCount&&model->assets[terrainSelected].terrain){
        char name[32];snprintf(name,32,"%s",model->assets[terrainSelected].name);char16_t title[128],choices[3][128]={u"Texture...",u"Convert to 3DO (model library)",u"Convert to walkstruct (keep in level)"};stdString_CharToWchar(title,name,128);
        int choice=chooseList(title,choices,3,0),asset=-1;for(int a=0;a<model->assetCount;a++)if(model->assets[a].terrain&&!strcmp(name,model->assets[a].name))asset=a;
        if(asset>=0&&choice==0&&!netEditPending()){
            int oldMaterial=currentMaterial,oldObjects=browserObjects;for(int i=0;i<model->meshCount;i++)if(model->meshes[i].asset==asset){currentMaterial=model->meshes[i].geometry.f[0].material;break;}
            browserObjects=0;materialMenu(1);int material=currentMaterial;currentMaterial=oldMaterial;browserObjects=oldObjects;
            if(asset<model->assetCount&&model->assets[asset].terrain&&!strcmp(name,model->assets[asset].name)&&!netEditPending()){edCopyModel(candidate,model);for(int i=0;i<candidate->meshCount;i++)if(candidate->meshes[i].asset==asset||(candidate->assets[asset].terrainGroup&&candidate->assets[candidate->meshes[i].asset].terrainGroup==candidate->assets[asset].terrainGroup))for(int f=0;f<candidate->meshes[i].geometry.count;f++)candidate->meshes[i].geometry.f[f].material=material;remember();if(commit())say("Terrain texture updated. Ctrl+Z: undo.");}
        }else if(asset>=0&&choice>0)terrainConvert(asset,choice==2);
    }
    builderMenuEnd(state);
}
static int terrainControl(EdVec origin,EdVec direction,int fire,int middle,int shift,int wheel,double delta,int smooth=0){
    if(terrainStroke&&!grabbing){terrainStroke=0;terrainElapsed=0;}
    int asset=terrainPick(origin,direction,&terrainHover);if(asset>=0)terrainSelected=asset;
    if(wheel)terrainRadius=fmax(ED_GRID*2,fmin(32,terrainRadius*pow(1.2,wheel>0?1:-1)));
    if(middle){terrainStroke=grabbing=0;terrainMenu();return 1;}
    if(fire&&!fireDown&&!terrainStroke){
        if(asset<0){if(!smooth)terrainCreate();fireDown=1;return 1;}
        remember();terrainStroke=grabbing=1;terrainElapsed=0;
    }
    if(terrainStroke){terrainElapsed+=fmin(.1,fmax(0,delta));
        if(asset>=0&&(terrainElapsed>=.05||!fire)){edCopyModel(candidate,model);if(smooth?edTerrainSmooth(candidate,asset,terrainHover.point,terrainRadius,fmin(1,4*terrainElapsed)):edTerrainSculpt(candidate,asset,terrainHover.point,terrainRadius,(shift?-1:1)*.4*terrainElapsed))commit();terrainElapsed=0;}
        if(!fire){terrainStroke=grabbing=0;terrainElapsed=0;}
    }
    fireDown=fire;grabDown=editButton(INPUT_FUNC_FIRE2);return 1;
}
static void terrainDraw(int color){
    if(tool!=7||terrainHover.cell<0||terrainHover.cell>=model->meshCount)return;
    int asset=model->meshes[terrainHover.cell].asset;if(!model->assets[asset].terrain)return;EdVec n=edNormal(&model->meshes[terrainHover.cell].geometry.f[terrainHover.face]);
    EdVec u=edCross(fabs(n.z)<.9?edV(0,0,1):edV(0,1,0),n);u=edScale(u,1/sqrt(edDot(u,u)));EdVec v=edCross(n,u),previous={};int previousValid=0;
    for(int i=0;i<=64;i++){double angle=i*6.283185307179586/64;EdVec p=edAdd(terrainHover.point,edAdd(edScale(u,cos(angle)*terrainRadius),edScale(v,sin(angle)*terrainRadius)));EdHit hit;
        int valid=edMeshRay(model,asset,edAdd(p,edScale(n,2048)),edScale(n,-1),0,&hit);if(valid){p=edAdd(hit.point,edScale(n,.003));if(previousValid)line(previous,p,color);previous=p;}previousValid=valid;
    }
}

static int terrainSelfTest(SithThing *player){
    clearHistory();tool=7;fireDown=grabDown=0;picks.count=0;
    EdVec origin=edV(0,0,.5),direction=edV(0,0,-1);if(!edRay(model,origin,direction,&hover))return 0;
    if(!terrainCreate()||terrainSelected<0)return 0;int asset=terrainSelected;
    if(edTerrainObject(model,model->objectCount-1)!=asset||!sithTemplate_GetTemplate(model->assets[asset].name))return 0;
    SithThing *terrainTemplate=sithTemplate_GetTemplate(model->assets[asset].name);if(!(terrainTemplate->flags&SITH_TF_EARTH)||(terrainTemplate->flags&SITH_TF_METAL))return 0;
    // Repeated brush updates must work even with no unused live-model slots.
    SithWorld *world=sithWorld_g_pCurrentWorld;int modelCount=world->numModels,capacity=world->sizeModels;
    world->sizeModels=modelCount;
    for(int tick=0;tick<80;tick++){
        edCopyModel(candidate,model);
        if(!edTerrainSculpt(candidate,asset,edV(0,0,0),.5,tick%2?-.001:.001)||!commit()||world->numModels!=modelCount){world->sizeModels=capacity;return 0;}
    }
    world->sizeModels=capacity;
    EdModel flat;edCopyModel(&flat,model);int history=undoCount;
    terrainControl(origin,direction,1,0,0,0,.1);terrainControl(origin,direction,1,0,0,0,.1);terrainControl(origin,direction,0,0,0,0,0);
    if(edModelsEqual(&flat,model)||undoCount!=history+1||!historyStep(0)||!edModelsEqual(&flat,model)||!historyStep(1))return 0;
    EdModel sculpted;edCopyModel(&sculpted,model);
    history=undoCount;terrainControl(origin,direction,1,0,0,0,.1,1);terrainControl(origin,direction,1,0,0,0,.1,1);terrainControl(origin,direction,0,0,0,0,0,1);
    if(edModelsEqual(model,&sculpted)||undoCount!=history+1||!historyStep(0)||!edModelsEqual(model,&sculpted))return 0;

    if(!terrainConvert(asset,1)||model->assets[asset].terrain||!historyStep(0)||!edModelsEqual(&sculpted,model))return 0;
    int objects=model->objectCount;if(!terrainConvert(asset,0)||model->objectCount!=objects-1||!historyStep(0)||!edModelsEqual(&sculpted,model))return 0;
    if(!terrainDelete(asset)||model->assetCount||model->meshCount||model->objectCount!=objects-1||!historyStep(0)||!edModelsEqual(&sculpted,model))return 0;
    if(getenv("GEOSITH_TERRAIN_GROUP_TEST")){
        edDefault(candidate);if(!edExtrude(candidate,0,4,edV(0,1,0))||!commit())return 0;clearHistory();picks.count=0;
        for(int c=0;c<model->count;c++)for(int f=0;f<model->c[c].count;f++)if(edNormal(&model->c[c].f[f]).z>.99)picks.hits[picks.count++]={c,f,-1,-1};
        if(!terrainCreate()||model->assetCount!=2||model->objectCount!=2)return 0;
        for(int o=0;o<2;o++)if(objectSector(model,model->objects[o].position)!=o)return 0;
        EdModel grouped;edCopyModel(&grouped,model);
        if(!terrainConvert(0,1)||model->assets[0].terrain||model->assets[1].terrain||!historyStep(0)||!edModelsEqual(model,&grouped))return 0;
        if(!terrainDelete(1)||model->assetCount||model->meshCount||model->objectCount||!historyStep(0)||!edModelsEqual(model,&grouped))return 0;
        return writeEditorGob(model,"Episode/sithed_terrain_group_test.gob","Terrain group test",0);
    }
    return writeEditorGob(model,"Episode/sithed_terrain_test.gob","Terrain integration test",0);
}
