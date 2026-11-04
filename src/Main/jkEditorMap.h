/* Included by jkEditor.c: shares its validated, host-authoritative transactions. */
static EdBuffer<unsigned char> mapSelected,mapGrabSelected;
static EdModel *mapClipboard;
static EdModel *mapPending;
static int mapUnplaced,mapDrawing,mapInsertDown,mapBackDown;
static int mapCleaveMode,mapCleaveDown,mapDeleteDown,mapCleaveDragged;
static EdVec mapCleaveStartPoint;
static EdVec mapPendingCenter,mapDrawPoints[ED_VERTS];
static double mapFloorZ;
static EdVec mapCenter,mapGrabPoint,mapClipboardCenter,mapLastDelta;
static double mapZoom,mapX,mapY,mapStartX,mapStartY;
static int mapWidth=640,mapHeight=480,mapLeft,mapRight,mapCopyDown,mapPasteDown,mapHistoryDown,mapBox,mapDragging;
static uint32_t mapMoveType;
static int mapGeometryDrag;

/* Top-down handles include every height at the picked XY location. Shared
   face-loop vertices/edges are deduplicated by the normal selection API. */
static void mapAddHit(EdSelection *list,EdHit hit){
    EdSelection next=*list;if(edSelectionToggle(model,&next,hit)&&next.count>list->count)*list=next;
}
static void mapPickGeometry(EdVec point,EdSelection *result){
    result->count=0;double best=8/mapZoom;EdVec target=point;int vertex=0;
    for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++){
        EdVec p=model->c[s].f[f].v[v].p;double d=hypot(p.x-point.x,p.y-point.y);
        if(d<best){best=d;target=p;vertex=1;}
    }
    double edgeBest=6/mapZoom;EdVec edgeA={0},edgeB={0};int edge=0;
    if(!vertex)for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++){
        const EdFace *face=&model->c[s].f[f];for(int v=0;v<face->count;v++){
            EdVec a=face->v[v].p,b=face->v[(v+1)%face->count].p;double dx=b.x-a.x,dy=b.y-a.y,den=dx*dx+dy*dy;if(den<ED_EPS*ED_EPS)continue;
            double t=fmax(0,fmin(1,((point.x-a.x)*dx+(point.y-a.y)*dy)/den));double d=hypot(point.x-a.x-t*dx,point.y-a.y-t*dy);
            if(d<edgeBest){edgeBest=d;edgeA=a;edgeB=b;edge=1;}
        }
    }
    if(!vertex&&!edge)return;
    for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++){
        const EdFace *face=&model->c[s].f[f];for(int v=0;v<face->count;v++){
            EdVec a=face->v[v].p,b=face->v[(v+1)%face->count].p;EdHit h={s,f,-1,-1};
            if(vertex){if(hypot(a.x-target.x,a.y-target.y)>ED_EPS)continue;h.vertex=v;h.point=a;}
            else {if(!((hypot(a.x-edgeA.x,a.y-edgeA.y)<ED_EPS&&hypot(b.x-edgeB.x,b.y-edgeB.y)<ED_EPS)||(hypot(b.x-edgeA.x,b.y-edgeA.y)<ED_EPS&&hypot(a.x-edgeB.x,a.y-edgeB.y)<ED_EPS)))continue;h.edge=v;h.point=edScale(edAdd(a,b),.5);}
            mapAddHit(result,h);
        }
    }
}

static EdVec mapPoint(double x,double y){return edV(mapCenter.x+(x-mapWidth*.5)/mapZoom,mapCenter.y-(y-mapHeight*.5)/mapZoom,0);}
static void mapSize(void){rdCanvas *canvas=Video_pCanvas;
#ifdef SDL2_RENDER
    canvas=Video_pCanvasOverlayMap;
#endif
    if(canvas&&canvas->pVBuffer){mapWidth=canvas->pVBuffer->format.width;mapHeight=canvas->pVBuffer->format.height;}
}
static void mapCancel(void){mapUnplaced=mapDrawing=mapBox=mapCleaveMode=mapCleaveDragged=0;if(mapDragging)grabbing=0;mapDragging=0;}
static void mapEscape(void){mapCancel();say("Map drawing / placement / cleave cancelled.");}
static void mapInvalidate(void){edZero(mapSelected,0,sizeof(mapSelected));mapCancel();}
static int mapAllocatePending(void){if(!mapPending)mapPending=edCalloc(1,sizeof(*mapPending));if(!mapPending){say("Could not allocate a sector preview.");return 0;}return 1;}
static EdVec mapPlacementDelta(void){EdVec delta=edSnapPoint(edSub(mapPoint(mapX,mapY),mapPendingCenter));delta.z=0;return delta;}
static void mapBeginPlacement(void){
    if(!mapClipboard||!mapClipboard->count||!mapAllocatePending())return;mapCancel();mapPending->count=mapClipboard->count;edCopy(mapPending->c,mapClipboard->c,mapClipboard->count*sizeof(EdCell));mapPendingCenter=mapClipboardCenter;mapUnplaced=1;
    say("Unplaced sectors: move the mouse, LMB to place and merge, Escape to cancel.");
}
static void mapInsert(void){
    if(!mapAllocatePending())return;mapCancel();edDefault(mapPending);
    for(int f=0;f<mapPending->c[0].count;f++){EdFace *face=&mapPending->c[0].f[f];face->material=currentMaterial;for(int v=0;v<face->count;v++)face->v[v].p.z+=mapFloorZ;edMapFace(face);}
    mapPendingCenter=edV(0,0,0);mapUnplaced=1;say("New sector preview: move the mouse, LMB to place, Escape to cancel.");
}
static int mapPlace(EdVec delta){
    EdBuffer<unsigned char> selected;
    if(netEditPending()){say("Wait for the host before placing the sector.");return 0;}
    if(!edMapPaste(candidate,model,mapPending,delta,selected)){say(edMapPlacementFailure);return 0;}
    remember();if(!commit())return 0;edCopy(mapSelected,selected,sizeof(mapSelected));mapCancel();say("Sectors placed and merged. Ctrl+Z: undo.");return 1;
}
static int mapDrawCandidate(EdVec *point,int *closing){
    *point=edSnapPoint(mapPoint(mapX,mapY));*closing=mapDrawing&&hypot(point->x-mapDrawPoints[0].x,point->y-mapDrawPoints[0].y)*mapZoom<=8;
    if(*closing)*point=mapDrawPoints[0];return edMapDrawValid(mapDrawPoints,mapDrawing,*point,*closing);
}
static void mapBackspace(int down){
    if(down&&!mapBackDown&&mapDrawing){mapDrawing--;say(mapDrawing?"Last vertex removed. Continue drawing; Backspace removes another vertex.":"Drawing cleared. Shift+LMB starts a new outline.");}
    mapBackDown=down;
}
static void mapDrawClick(void){
    EdVec point;int closing;
    if(!mapDrawCandidate(&point,&closing)){say("Wall rejected: keep the outline convex and avoid crossing walls. Backspace removes the last vertex.");return;}
    if(mapDrawing){
        if(closing){if(mapDrawing<3){say("Draw at least three walls before closing the shape.");return;}
            if(!mapAllocatePending()||!edMapPolygon(mapPending,mapDrawPoints,mapDrawing,mapFloorZ,mapFloorZ+1,currentMaterial)){say("Cannot close this shape: walls must form a simple, nonzero-area polygon.");return;}
            mapPlace(edV(0,0,0));return;}
        if(edDot(edSub(point,mapDrawPoints[mapDrawing-1]),edSub(point,mapDrawPoints[mapDrawing-1]))<ED_EPS)return;
    }
    if(mapDrawing>=ED_VERTS){say("Drawing limit reached. Click the first vertex to close, or Escape to cancel.");return;}
    mapDrawPoints[mapDrawing++]=point;mapBox=0;say("Drawing: click for walls, click first vertex to close. Backspace removes a vertex; Escape cancels.");
}
static void mapBeginCleave(void){
    if(netEditPending()||mapDragging){say("Finish the current edit before cleaving sectors.");return;}
    int count=0;for(int s=0;s<model->count;s++)count+=mapSelected[s]!=0;if(!count){say("Select sectors, then press C to cleave.");return;}
    mapCancel();mapCleaveMode=1;say("Cleave: click and drag a line across selected sectors, or click two endpoints. Escape cancels.");
}
static int mapFinishCleave(EdVec end){
    EdBuffer<unsigned char> selected;
    if(netEditPending()){say("Wait for the host before cleaving sectors.");return 0;}
    if(!edMapCleave(candidate,model,mapSelected,mapCleaveStartPoint,end,selected)){say("Cleave not placed: draw through selected sectors; keep two valid convex halves.");return 0;}
    remember();if(!commit())return 0;edCopy(mapSelected,selected,sizeof(mapSelected));mapCancel();say("Selected sectors cleaved with connecting adjoins. Ctrl+Z: undo.");return 1;
}
static int mapDeleteSelected(SithThing *player){
    if(netEditPending()||mapDragging){say("Finish the current edit before deleting sectors.");return 0;}
    if(!edMapDelete(candidate,model,mapSelected)){say("Select sectors to delete, keeping at least one sector in the level.");return 0;}
    remember();if(!commit())return 0;mapInvalidate();picks.count=0;hover.cell=-1;
    if(!active&&player&&objectSector(model,vec(player->position))<0){nextSpawn=0;teleportWalkplayer(player);}
    say("Selected sectors deleted; former adjoins closed. Ctrl+Z: undo.");return 1;
}
static void mapStopPlayer(SithThing *p){
    p->moveType=SITH_MT_NONE;
    rdVector_Zero3(&p->physicsParams.vel);rdVector_Zero3(&p->physicsParams.acceleration);
    rdVector_Zero3(&p->physicsParams.deltaVelocity);rdVector_Zero3(&p->physicsParams.angularVelocity);
}
static void mapOpen(SithThing *p){
    mapSize();mapCenter=vec(p->position);mapZoom=fmin(mapWidth,mapHeight)/3.0;mapX=mapWidth*.5;mapY=mapHeight*.5;
    mapMoveType=p->moveType;if(p->attach_flags)sithThing_DetachThing(p);mapStopPlayer(p);
    mapActive=1;mapCancel();grabbing=groupGrab=cleaving=objectGrabbing=objectRotating=lightGrabbing=0;
    mapFloorZ=floor(p->position.z);for(int s=0;s<model->count;s++)if(edContains(&model->c[s],vec(p->position))){mapFloorZ=1e30;for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++)mapFloorZ=fmin(mapFloorZ,model->c[s].f[f].v[v].p.z);break;}
    mapFloorZ=edSnapPoint(edV(0,0,mapFloorZ)).z;mapInsertDown=stdControl_ReadKey(DIK_INSERT,NULL)!=0;
    mapBackDown=stdControl_ReadKey(DIK_BACK,NULL)!=0;
    mapCleaveDown=stdControl_ReadKey(DIK_C,NULL)!=0;mapDeleteDown=stdControl_ReadKey(DIK_DELETE,NULL)!=0;
    mapLeft=stdControl_ReadKey(KEY_MOUSE_B1,NULL)!=0;mapRight=stdControl_ReadKey(KEY_MOUSE_B2,NULL)!=0;
    mapCopyDown=stdControl_ReadKey(DIK_C,NULL)!=0;mapPasteDown=stdControl_ReadKey(DIK_V,NULL)!=0;mapHistoryDown=0;
    /* Share exact 3D handles, rather than expanding them to entire sectors. */
    edZero(mapSelected,0,sizeof(mapSelected));
    say("Map: LMB select, C cleave, Delete selected, Shift+LMB draw, Insert new, Ctrl+C/V preview.");
}
static void mapClose(SithThing *p){
    if(!mapActive)return;mapActive=0;if(p){p->moveType=mapMoveType;rdVector_Zero3(&p->physicsParams.vel);rdVector_Zero3(&p->physicsParams.acceleration);}
    mapCancel();grabbing=0;
    /* Latch held keys so returning cannot start a 3D gesture or jump. */
    fireDown=editButton(INPUT_FUNC_FIRE1);grabDown=editButton(INPUT_FUNC_FIRE2);toggleDown=stdControl_ReadKey(DIK_F8,NULL)!=0;
    homeDown=stdControl_ReadKey(DIK_HOME,NULL)!=0;qDown=stdControl_ReadKey(DIK_Q,NULL)!=0;
    middleDown=stdControl_ReadKey(KEY_MOUSE_B3,NULL)!=0;deleteDown=stdControl_ReadKey(DIK_DELETE,NULL)!=0;attributesDown=stdControl_ReadKey(DIK_RETURN,NULL)!=0;
    edResetDoubleTap(&jumpTap,sithControl_GetKey(INPUT_FUNC_JUMP,NULL));edResetDoubleTap(&selectionTap,fireDown);
    hover.cell=-1;say("3D editor. Tab: overhead map. Q: flight collision.");
}
static void mapCopy(void){
    int count=0;for(int s=0;s<model->count;s++)count+=mapSelected[s]!=0;if(!count){say("Select sectors to copy.");return;}
    if(!mapClipboard)mapClipboard=edCalloc(1,sizeof(*mapClipboard));if(!mapClipboard){say("Could not allocate sector clipboard.");return;}
    mapClipboard->count=0;mapClipboardCenter=edV(0,0,0);
    /* Keep original portal status in the private clipboard. Paste captures
       those attributes before rebuilding links; external links then become
       solid, while internal visible/impassable adjoins keep their settings. */
    for(int s=0;s<model->count;s++)if(mapSelected[s]){EdCell *cell=&mapClipboard->c[mapClipboard->count++];*cell=model->c[s];mapClipboardCenter=edAdd(mapClipboardCenter,edMapCenter(cell));}
    mapClipboardCenter=edScale(mapClipboardCenter,1.0/count);mapBeginPlacement();
}
static void mapInput(double dx,double dy,int wheel,int left,int right,int space,int additive,int copy,int paste,int history){
    if(mapDragging&&!grabbing)mapDragging=0; /* A remote edit cancelled the gesture. */
    if(space){mapCenter.x-=dx/mapZoom;mapCenter.y+=dy/mapZoom;}
    else{mapX=fmax(0,fmin(mapWidth-1,mapX+dx));mapY=fmax(0,fmin(mapHeight-1,mapY+dy));}
    if(wheel&&!mapDragging&&!mapBox){EdVec before=mapPoint(mapX,mapY);mapZoom=fmax(4,fmin(32768,mapZoom*pow(1.2,fmax(-12,fmin(12,wheel)))));EdVec after=mapPoint(mapX,mapY);mapCenter=edAdd(mapCenter,edSub(before,after));}
    int busy=netEditPending();
    if(history&&!mapHistoryDown&&!busy&&!mapDragging){historyStep(history==2);mapInvalidate();}
    if(copy&&!mapCopyDown)mapCopy();
    if(paste&&!mapPasteDown&&!mapDragging)mapBeginPlacement();
    if(mapCleaveMode){
        EdVec end=edSnapPoint(mapPoint(mapX,mapY));
        if(!space&&!right){
            if(left&&!mapLeft){
                if(mapCleaveMode==1){mapCleaveStartPoint=end;mapCleaveMode=2;mapCleaveDragged=0;mapStartX=mapX;mapStartY=mapY;}
                else mapFinishCleave(end);
            }else if(mapCleaveMode==2){
                if(left&&hypot(mapX-mapStartX,mapY-mapStartY)>4)mapCleaveDragged=1;
                if(!left&&mapLeft&&mapCleaveDragged)mapFinishCleave(end);
            }
        }goto buttons;
    }
    if(mapUnplaced||mapDrawing||additive&&left&&!mapLeft&&!space&&!right){
        if(left&&!mapLeft&&!space&&!right){if(mapUnplaced)mapPlace(mapPlacementDelta());else mapDrawClick();}
        goto buttons;
    }
    if(left&&!mapLeft&&!space&&!right){mapBox=1;mapStartX=mapX;mapStartY=mapY;}
    if(!left&&mapLeft&&mapBox){
        EdVec a=mapPoint(mapStartX,mapStartY),b=mapPoint(mapX,mapY);int box=fabs(mapX-mapStartX)>4||fabs(mapY-mapStartY)>4;
        EdSelection hit={0};if(!box)mapPickGeometry(b,&hit);
        edZero(mapSelected,0,sizeof(mapSelected));
        if(hit.count){for(int i=0;i<hit.count;i++)if(!edSelectionToggle(model,&picks,hit.hits[i]))say("Selection limit reached.");}
        else {picks.count=0;for(int s=0;s<model->count;s++)if(box?edMapSectorBox(&model->c[s],a.x,a.y,b.x,b.y):edMapSectorPoint(&model->c[s],b.x,b.y))mapSelected[s]=1;}mapBox=0;
    }
    if(right&&!mapRight&&!space&&!left&&!busy){int any=0;for(int s=0;s<model->count;s++)any|=mapSelected[s];
        if(any||picks.count){remember();edCopyModel(snapshot,model);mapGeometryDrag=!any&&picks.count;grabPicks=picks;edCopy(mapGrabSelected,mapSelected,sizeof(mapSelected));mapGrabPoint=mapPoint(mapX,mapY);mapLastDelta=edV(0,0,0);mapDragging=grabbing=1;}}
    if(right&&mapDragging&&!space){EdVec delta=edSnapPoint(edSub(mapPoint(mapX,mapY),mapGrabPoint));EdBuffer<unsigned char> selected;
        /* Each frame starts from the gesture snapshot, avoiding cumulative drift. */
        if(edDot(edSub(delta,mapLastDelta),edSub(delta,mapLastDelta))>ED_EPS*ED_EPS){
            int valid=mapGeometryDrag?edMoveSelectionXY(candidate,snapshot,&grabPicks,delta):edMapTranslate(candidate,snapshot,mapGrabSelected,delta,0,selected);
            if(valid&&commit()){if(mapGeometryDrag)edSelectionRemap(snapshot,model,&grabPicks,delta,&picks);else edCopy(mapSelected,selected,sizeof(mapSelected));mapLastDelta=delta;}
        }
    }
    if(!right&&mapRight){mapDragging=grabbing=0;}
buttons:
    mapLeft=left;mapRight=right;mapCopyDown=copy;mapPasteDown=paste;mapHistoryDown=history;
}
static void mapControl(SithThing *p){
    jkControl_HandleScreenshot();mapStopPlayer(p);mapSize();
    int tab=stdControl_ReadKey(DIK_TAB,NULL)!=0;if(tab&&!tabDown){tabDown=tab;mapClose(p);return;}tabDown=tab;
    int ctrl=stdControl_ReadKey(DIK_LCONTROL,NULL)||stdControl_ReadKey(DIK_RCONTROL,NULL),shift=stdControl_ReadKey(DIK_LSHIFT,NULL)||stdControl_ReadKey(DIK_RSHIFT,NULL);
    int c=stdControl_ReadKey(DIK_C,NULL)!=0,deleting=stdControl_ReadKey(DIK_DELETE,NULL)!=0;
    if(c&&!mapCleaveDown&&!ctrl)mapBeginCleave();mapCleaveDown=c;
    if(deleting&&!mapDeleteDown)mapDeleteSelected(p);mapDeleteDown=deleting;
    int inserting=stdControl_ReadKey(DIK_INSERT,NULL)!=0;if(inserting&&!mapInsertDown)mapInsert();mapInsertDown=inserting;
    mapBackspace(stdControl_ReadKey(DIK_BACK,NULL)!=0);
    mapInput(stdControl_aAxisStates[AXIS_MOUSE_X],stdControl_aAxisStates[AXIS_MOUSE_Y],stdControl_aAxisStates[AXIS_MOUSE_Z],stdControl_ReadKey(KEY_MOUSE_B1,NULL)!=0,stdControl_ReadKey(KEY_MOUSE_B2,NULL)!=0,stdControl_ReadKey(DIK_SPACE,NULL)!=0,shift,
        ctrl&&stdControl_ReadKey(DIK_C,NULL),ctrl&&stdControl_ReadKey(DIK_V,NULL),ctrl&&stdControl_ReadKey(DIK_Y,NULL)?2:ctrl&&stdControl_ReadKey(DIK_Z,NULL)?1:0);
}
static int mapColor(int r,int g,int b){rdColor24 *colors=(rdColor24*)stdDisplay_masterPalette;int color=1+stdColor_FindClosest(colors+1,255,r,g,b);
    if(highlightCanvas->pVBuffer->format.format.bpp!=8)color=stdColor_Indexed8ToRGB16(color,colors,&highlightCanvas->pVBuffer->format.format);return color;}
static void mapLine(double x,double y,double bx,double by,int color,int width){
    /* Clip in double precision before converting far-away map coordinates. */
    double dx=bx-x,dy=by-y,lo=0,hi=1,p[4]={-dx,dx,-dy,dy},q[4]={x,mapWidth-1-x,y,mapHeight-1-y};
    for(int i=0;i<4;i++){if(fabs(p[i])<1e-12){if(q[i]<0)return;}else{double t=q[i]/p[i];if(p[i]<0)lo=fmax(lo,t);else hi=fmin(hi,t);}}if(lo>hi)return;
    screenLine((int)(x+dx*lo),(int)(y+dy*lo),(int)(x+dx*hi),(int)(y+dy*hi),color,width);
}
static void mapWorldLine(EdVec a,EdVec b,int color,int width){mapLine((a.x-mapCenter.x)*mapZoom+mapWidth*.5,mapHeight*.5-(a.y-mapCenter.y)*mapZoom,(b.x-mapCenter.x)*mapZoom+mapWidth*.5,mapHeight*.5-(b.y-mapCenter.y)*mapZoom,color,width);}
static void mapMarker(EdVec point,int first,int color){double x=(point.x-mapCenter.x)*mapZoom+mapWidth*.5,y=mapHeight*.5-(point.y-mapCenter.y)*mapZoom;
    if(x>=-12&&y>=-12&&x<mapWidth+12&&y<mapHeight+12)rdPrimit2_DrawClippedCircle(highlightCanvas,(int)x,(int)y,first?8:5,15,color,-1);
}
static void mapDraw(void){
    mapSize();tVBuffer *buffer=highlightCanvas->pVBuffer;int black=mapColor(0,0,0),grid=mapColor(48,48,48),green=mapColor(32,255,64),selected=mapColor(255,48,48),cursor=mapColor(240,240,240);
    /* The ordinary HUD (including the crosshair and chat) was drawn before
       this overlay. Keep the map as a dedicated editing canvas. */
    if(Video_pMenuBuffer&&Video_pMenuBuffer!=buffer)stdDisplay_VBufferFill(Video_pMenuBuffer,Video_fillColor,NULL);
    stdDisplay_VBufferFill(buffer,black,NULL);stdDisplay_VBufferLock(buffer);
    double spacing=ED_GRID;while(spacing*mapZoom<12)spacing*=2;EdVec corner=mapPoint(0,0),oppositeCorner=mapPoint(mapWidth,mapHeight);
    for(double x=ceil(corner.x/spacing)*spacing;x<=oppositeCorner.x;x+=spacing){double sx=(x-mapCenter.x)*mapZoom+mapWidth*.5;mapLine(sx,0,sx,mapHeight-1,grid,1);}
    for(double y=ceil(oppositeCorner.y/spacing)*spacing;y<=corner.y;y+=spacing){double sy=mapHeight*.5-(y-mapCenter.y)*mapZoom;mapLine(0,sy,mapWidth-1,sy,grid,1);}
    for(int pass=0;pass<2;pass++)for(int s=0;s<model->count;s++)if((mapSelected[s]!=0)==pass)for(int f=0;f<model->c[s].count;f++){
        const EdFace *face=&model->c[s].f[f];for(int v=0;v<face->count;v++){EdVec a=face->v[v].p,b=face->v[(v+1)%face->count].p;
            mapLine((a.x-mapCenter.x)*mapZoom+mapWidth*.5,mapHeight*.5-(a.y-mapCenter.y)*mapZoom,(b.x-mapCenter.x)*mapZoom+mapWidth*.5,mapHeight*.5-(b.y-mapCenter.y)*mapZoom,pass?selected:green,pass?3:1);}}
    for(int s=0;s<model->count;s++)for(int f=0;f<model->c[s].count;f++)for(int v=0;v<model->c[s].f[f].count;v++)mapMarker(model->c[s].f[f].v[v].p,0,mapSelected[s]?selected:green);
    /* Draw exact 3D selections last so overlapping heights remain visible. */
    for(int i=0;i<picks.count;i++){
        EdHit h=picks.hits[i];if(h.cell<0||h.cell>=model->count||h.face<0||h.face>=model->c[h.cell].count)continue;
        const EdFace *face=&model->c[h.cell].f[h.face];
        if(h.vertex>=0){if(h.vertex<face->count)mapMarker(face->v[h.vertex].p,1,selected);}
        else for(int v=0;v<face->count;v++)if(h.edge<0||h.edge==v){mapWorldLine(face->v[v].p,face->v[(v+1)%face->count].p,selected,3);mapMarker(face->v[v].p,0,selected);mapMarker(face->v[(v+1)%face->count].p,0,selected);}
    }
    int previewColor=mapColor(255,224,64);
    if(mapCleaveMode==2){EdVec end=edSnapPoint(mapPoint(mapX,mapY));EdBuffer<unsigned char> cut;int valid=edMapCutMask(model,mapSelected,mapCleaveStartPoint,end,cut)>0;
        mapWorldLine(mapCleaveStartPoint,end,valid?previewColor:selected,3);mapMarker(mapCleaveStartPoint,1,previewColor);mapMarker(end,0,valid?previewColor:selected);
    }
    if(mapUnplaced){EdVec delta=mapPlacementDelta();for(int s=0;s<mapPending->count;s++)for(int f=0;f<mapPending->c[s].count;f++){EdFace *face=&mapPending->c[s].f[f];for(int v=0;v<face->count;v++)mapWorldLine(edAdd(face->v[v].p,delta),edAdd(face->v[(v+1)%face->count].p,delta),previewColor,3);}}
    if(mapDrawing){for(int i=0;i<mapDrawing;i++){if(i)mapWorldLine(mapDrawPoints[i-1],mapDrawPoints[i],previewColor,3);mapMarker(mapDrawPoints[i],!i,previewColor);}
        EdVec end;int closing,valid=mapDrawCandidate(&end,&closing);mapWorldLine(mapDrawPoints[mapDrawing-1],end,valid?previewColor:selected,valid?1:3);mapMarker(end,0,valid?cursor:selected);
    }
    if(mapBox){mapLine(mapStartX,mapStartY,mapX,mapStartY,cursor,1);mapLine(mapX,mapStartY,mapX,mapY,cursor,1);mapLine(mapX,mapY,mapStartX,mapY,cursor,1);mapLine(mapStartX,mapY,mapStartX,mapStartY,cursor,1);}
    mapLine(mapX-9,mapY,mapX+9,mapY,cursor,1);mapLine(mapX,mapY-9,mapX,mapY+9,cursor,1);
    char label[160];int count=0;for(int s=0;s<model->count;s++)count+=mapSelected[s]!=0;
    snprintf(label,sizeof(label),"MAP | %d sectors | %d sectors / %d handles selected | Tab: 3D editor",model->count,count,picks.count);editorDrawText(buffer,jkGui_stdFonts[0],12,12,mapWidth-24,label,1);
    editorDrawText(buffer,jkGui_stdFonts[0],12,34,mapWidth-24,"LMB select / box | C cleave | Delete selected | Shift+LMB draw | Insert new | RMB move | Space pan | Wheel zoom | Ctrl+C/V",1);
    editorDrawText(buffer,jkGui_stdFonts[0],12,mapHeight-28,mapWidth-24,message,1);stdDisplay_VBufferUnlock(buffer);
}
static int mapSelfTest(SithThing *p){
    EdModel *saved=edAlloc(sizeof(*saved)),*savedSnapshot=edAlloc(sizeof(*savedSnapshot));if(!saved||!savedSnapshot){edFree(saved);edFree(savedSnapshot);return 0;}edCopyModel(saved,model);edCopyModel(savedSnapshot,snapshot);EdSelection old=picks;uint32_t movement=p->moveType;rdVector3 position=p->position;
    clearHistory();picks.count=1;picks.hits[0]=EdHit{0,0,0,-1};mapOpen(p);
    int retained=picks.count==1&&picks.hits[0].vertex==0&&!mapSelected[0];mapClose(p);retained=retained&&picks.count==1;
    picks.hits[0]=EdHit{0,4,-1,-1};mapOpen(p);retained=retained&&picks.count==1&&picks.hits[0].face==4&&!mapSelected[0];
    picks.count=0;mapCenter=edMapCenter(&model->c[0]);mapZoom=fmin(mapWidth,mapHeight)/8.0;mapLeft=mapRight=0;
    /* Pick a complete XY vertex column and edge, drag through the real map
       input path, then restore via shared undo before the sector tests. */
    for(int kind=0;kind<2;kind++){
        EdVec a=model->c[0].f[0].v[0].p,b=model->c[0].f[0].v[1].p;
        EdVec target=kind?edScale(edAdd(a,b),.5):a;
        mapX=(target.x-mapCenter.x)*mapZoom+mapWidth*.5;mapY=mapHeight*.5-(target.y-mapCenter.y)*mapZoom;
        mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        retained=retained&&picks.count>=2&&(kind?picks.hits[0].edge>=0:picks.hits[0].vertex>=0);
        EdSelection original=picks;
        mapInput(0,0,0,0,1,0,0,0,0,0);mapInput(mapZoom*ED_GRID,0,0,0,1,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        retained=retained&&edValidate(model)&&undoCount==1&&picks.count>0;
        retained=retained&&historyStep(0)&&edModelsEqual(model,saved)&&picks.count==original.count;
        clearHistory();picks.count=0;
    }
    rdMatrix34 orientation=p->orient;int oldX=stdControl_aAxisStates[AXIS_MOUSE_X],oldY=stdControl_aAxisStates[AXIS_MOUSE_Y];
    stdControl_aAxisStates[AXIS_MOUSE_X]=15;stdControl_aAxisStates[AXIS_MOUSE_Y]=10;p->physicsParams.vel.x=1;mapControl(p);
    int isolated=!edCompare(&orientation,&p->orient,sizeof(orientation))&&rdVector_IsZero3(&p->physicsParams.vel)&&p->moveType==SITH_MT_NONE;
    stdControl_aAxisStates[AXIS_MOUSE_X]=oldX;stdControl_aAxisStates[AXIS_MOUSE_Y]=oldY;mapX=mapWidth*.5;mapY=mapHeight*.5;
    mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);int ok=retained&&isolated&&mapActive&&mapSelected[0]&&p->moveType==SITH_MT_NONE;
    mapInput(0,0,0,0,0,0,0,1,0,0);mapInput(mapZoom*3,0,0,0,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,1,0);
    ok=ok&&mapUnplaced&&model->count==saved->count&&!undoCount;
    mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    ok=ok&&model->count==saved->count+1&&undoCount==1&&edValidate(model);
    mapInput(20-mapX,80-mapY,0,0,0,0,0,0,0,0);mapInput(0,0,0,1,0,0,0,0,0,0);
    mapInput(mapWidth-40,mapHeight-160,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    int selectedCount=0;for(int s=0;s<model->count;s++)selectedCount+=mapSelected[s]!=0;ok=ok&&selectedCount==model->count;
    mapInput(0,0,0,0,1,0,0,0,0,0);mapInput(0,-mapZoom*ED_GRID,0,0,1,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    ok=ok&&undoCount==2&&edValidate(model)&&historyStep(0)&&historyStep(0)&&model->count==saved->count;
    mapInsert();ok=ok&&mapUnplaced&&mapPending->c[0].count==6&&model->count==saved->count;mapEscape();ok=ok&&!mapUnplaced&&model->count==saved->count;
    mapInsert();mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);ok=ok&&model->count==saved->count+1&&historyStep(0);
    /* Shift starts drawing; subsequent plain clicks add walls. Closing the
       first vertex is one transaction, and Escape discards unfinished work. */
    mapX=mapWidth*.5+3*mapZoom;mapY=mapHeight*.5;
    mapInput(0,0,0,1,0,0,1,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);ok=ok&&mapDrawing==1;
    mapEscape();ok=ok&&!mapDrawing&&model->count==saved->count;
    mapInput(0,0,0,1,0,0,1,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    mapInput(mapZoom,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    mapInput(0,-mapZoom,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    mapInput(-mapZoom*.5,mapZoom*.75,0,0,0,0,0,0,0,0);EdVec rejected;int closing;ok=ok&&!mapDrawCandidate(&rejected,&closing);
    mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);ok=ok&&mapDrawing==3&&model->count==saved->count;
    mapBackspace(0);mapBackspace(1);ok=ok&&mapDrawing==2;mapBackspace(1);ok=ok&&mapDrawing==2;mapBackspace(0);
    mapInput(mapZoom*.5,-mapZoom*.75,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);ok=ok&&mapDrawing==3;
    mapInput(-mapZoom,mapZoom,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    ok=ok&&!mapDrawing&&model->count==saved->count+1&&edValidate(model)&&historyStep(0);
    /* C supports both two-click and click/drag lines, without selection boxes
       or player input leaking into the cleave gesture. */
    mapCenter=edMapCenter(&model->c[0]);edZero(mapSelected,0,sizeof(mapSelected));mapSelected[0]=1;
    mapBeginCleave();mapEscape();ok=ok&&!mapCleaveMode&&model->count==saved->count;
    mapBeginCleave();mapX=mapWidth*.5;mapY=mapHeight*.5+2*mapZoom;
    mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);ok=ok&&mapCleaveMode==2&&!mapBox;
    mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);ok=ok&&mapCleaveMode==2&&model->count==saved->count;
    mapInput(0,-4*mapZoom,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    ok=ok&&!mapCleaveMode&&model->count==saved->count+1&&edValidate(model)&&historyStep(0)&&historyStep(1)&&historyStep(0);
    mapBeginCleave();mapX=mapWidth*.5;mapY=mapHeight*.5+2*mapZoom;
    mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,-4*mapZoom,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    ok=ok&&!mapCleaveMode&&model->count==saved->count+1&&historyStep(0);
    mapX=mapWidth*.5+3*mapZoom;mapY=mapHeight*.5;mapInsert();mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
    ok=ok&&model->count==saved->count+1;
    mapSelected.clear();for(int s=0;s<model->count;s++)mapSelected[s]=1;int history=undoCount;ok=ok&&!mapDeleteSelected(p)&&undoCount==history;
    edZero(mapSelected,0,sizeof(mapSelected));mapSelected[model->count-1]=1;
    ok=ok&&mapDeleteSelected(p)&&model->count==saved->count&&historyStep(0)&&model->count==saved->count+1&&historyStep(1)&&model->count==saved->count&&historyStep(0)&&historyStep(0);
    EdVec before=mapPoint(mapX,mapY);mapInput(0,0,2,0,0,0,0,0,0,0);EdVec after=mapPoint(mapX,mapY);ok=ok&&edDot(edSub(before,after),edSub(before,after))<1e-10;
    double center=mapCenter.x;mapInput(20,0,0,0,0,1,0,0,0,0);ok=ok&&mapCenter.x<center;
    mapClose(p);ok=ok&&!mapActive&&p->moveType==movement&&rdVector_Dist3(&position,&p->position)<.001;
    edCopyModel(candidate,saved);int restored=commit();edCopyModel(snapshot,savedSnapshot);clearHistory();picks=old;edFree(saved);edFree(savedSnapshot);mapInvalidate();
    printf("GEOSITH_MAP %s: selection, placement, convex drawing, Backspace, click/drag cleaves, sector deletion, undo/redo, zoom/pan and player input isolation\n",ok&&restored?"PASS":"FAIL");fflush(stdout);return ok&&restored;
}
static int stressMap(SithThing *p){
    static int frame;if(!frame){stressRunning=1;stressRenderFrames=0;active=1;savedPhysics=p->physicsParams.flags;savedDebug=g_debugmodeFlags;savedThingFlags=p->flags;
        if(!mapSelfTest(p))exit(2);mapOpen(p);mapLeft=mapRight=0;mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        mapZoom=fmin(mapWidth,mapHeight)/8.0;mapInput(0,0,0,0,0,0,0,1,0,0);mapInput(mapZoom*3,0,0,0,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,1,0);mapCenter.x=1.5;
    }mapStopPlayer(p);
#ifdef SDL2_RENDER
    if(frame==16)std3D_ScreenshotWindow(getenv("GEOSITH_MAP_CAPTURE"));
    if(frame==22){char path[600];snprintf(path,sizeof(path),"%s.drawing.png",getenv("GEOSITH_MAP_CAPTURE"));std3D_ScreenshotWindow(path);}
    if(frame==24){char path[600];snprintf(path,sizeof(path),"%s.invalid.png",getenv("GEOSITH_MAP_CAPTURE"));std3D_ScreenshotWindow(path);}
    if(frame==28){char path[600];snprintf(path,sizeof(path),"%s.cleave.png",getenv("GEOSITH_MAP_CAPTURE"));std3D_ScreenshotWindow(path);}
    if(frame==32){char path[600];snprintf(path,sizeof(path),"%s.cleaved.png",getenv("GEOSITH_MAP_CAPTURE"));std3D_ScreenshotWindow(path);}
    if(getenv("GEOSITH_TEXT_PALETTE_TEST")&&(frame==38||frame==43)){char path[600];snprintf(path,sizeof(path),"%s.%s.png",getenv("GEOSITH_MAP_CAPTURE"),frame==38?"narsh":"returned");std3D_ScreenshotWindow(path);}
#endif
    if(frame==17){mapEscape();mapX=mapWidth*.5+mapZoom;mapY=mapHeight*.5;
        mapInput(0,0,0,1,0,0,1,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        mapInput(mapZoom,0,0,1,0,0,0,0,0,0);mapInput(0,0,0,0,0,0,0,0,0,0);
        mapInput(0,-mapZoom,0,1,0,0,0,0,0,0);mapInput(-mapZoom*.75,-mapZoom*.5,0,0,0,0,0,0,0,0);
    }
    if(frame==22)mapInput(mapZoom*.25,mapZoom*1.25,0,0,0,0,0,0,0,0);
    if(frame==25){mapEscape();mapCenter=edMapCenter(&model->c[0]);edZero(mapSelected,0,sizeof(mapSelected));mapSelected[0]=1;mapBeginCleave();
        mapX=mapWidth*.5;mapY=mapHeight*.5+2*mapZoom;mapInput(0,0,0,1,0,0,0,0,0,0);mapInput(0,-4*mapZoom,0,0,0,0,0,0,0,0);
    }
    if(frame==29)mapInput(0,0,0,1,0,0,0,0,0,0);
    if(getenv("GEOSITH_TEXT_PALETTE_TEST")&&(frame==34||frame==39)){edCopyModel(candidate,model);snprintf(candidate->header.colormap,128,"%s",frame==34?"01narsh.cmp":"dflt.cmp");if(!commit())exit(2);}
    if(frame++>=(getenv("GEOSITH_TEXT_PALETTE_TEST")?45:34)){printf("GEOSITH_STRESS PASS: overhead sector map, %d rendered frames\n",stressRenderFrames);fflush(stdout);exit(0);}return 1;
}
static int placedEditingSelfTest(void){
    EdModel *saved=edAlloc(sizeof(*saved));if(!saved)return 0;edCopyModel(saved,model);active=1;clearHistory();picks.count=0;int ok=1;
    int sectors[3]={0,saved->count/2,saved->count-1};
    for(int i=0;i<3&&ok;i++){
        edCopyModel(candidate,saved);ok=commit();clearHistory();int s=sectors[i],f=0;while(f<model->c[s].count&&model->c[s].f[f].neighbor>=0)f++;
        if(f==model->c[s].count)continue;remember();edCopyModel(candidate,model);ok=ok&&edExtrudeStep(candidate,s,f)&&commit()&&historyStep(0)&&historyStep(1);
        ok=ok&&sithWorld_g_pCurrentWorld->numSectors==model->count;
        for(int a=0;a<sithWorld_g_pCurrentWorld->numAdjoinsLoaded&&ok;a++){SithSurfaceAdjoin *join=&sithWorld_g_pCurrentWorld->adjoins[a];ok=join->mirror&&join->mirror->mirror==join&&join->sector==join->mirror->surface->pSector;}
    }
    edCopyModel(candidate,saved);ok=ok&&commit();clearHistory();remember();edCopyModel(candidate,model);
    ok=ok&&edCleaveSector(candidate,0,4,edV(0,1,0),edV(0,1,1))&&commit()&&historyStep(0)&&historyStep(1);
    edCopyModel(candidate,saved);ok=ok&&commit();clearHistory();remember();edCopyModel(candidate,model);
    ok=ok&&edCleave(candidate,0,4,edV(0,1,0),edV(0,1,1))&&commit()&&historyStep(0);
    SithThing *player=sithWorld_g_pCurrentWorld->pLocalPlayer;mapOpen(player);mapInsert();ok=ok&&mapPlace(edV(-1.5,0,0));mapInsert();ok=ok&&mapPlace(edV(-2.5,1.5,0));mapClose(player);
    edCopyModel(candidate,saved);int restored=commit();clearHistory();edFree(saved);return ok&&restored;
}
