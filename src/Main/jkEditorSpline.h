/* Editable spline controls use the normal document transaction path. */
static int splineSelected,splinePoint=-1,splineDrawing,splineDragging,splineHandle,splineStartDown,splineEnterDown,splineDragRight;
static EdSpline splineDraft;
static int splineLastShape=4;
static EdVec splineDragStart,splineDragDelta;
static EdDragTrace splineDragTrace;
static int splineIndex(int id){for(int i=0;i<model->splineCount;i++)if(model->splines[i].id==id)return i;return -1;}
static int splinePublish(const EdSpline &draft){
    if(netEditPending()){say("Wait for the host before changing a spline.");return 0;}
    int textureHeight=64;rdMaterial *material=editorMaterial(draft.material);
    if(material&&rdMaterial_EnsureDataForced(material)&&material->num_texinfo&&material->texinfos[0]&&material->texinfos[0]->texture_ptr){tVBuffer *texture=material->texinfos[0]->texture_ptr->texture_struct[0];if(texture&&texture->format.height>0)textureHeight=texture->format.height;}
    if(!edSplineRebuild(candidate,model,&draft,textureHeight)){say("Spline unchanged: curve is too tight, overlaps completely, or produces invalid geometry. Adjust width, handles or sector count.");return 0;}
    remember();if(!commit())return 0;splineLastShape=draft.sides;return 1;
}
static int splineDeleteSelected(void){
    int index=splineIndex(splineSelected);if(index<0)return 0;
    if(netEditPending()||splineDragging){say("Finish the current edit before deleting a spline or point.");return 1;}
    EdSpline draft=model->splines[index];
    if(splinePoint>=0&&splinePoint<draft.count&&draft.count>1){
        for(int i=splinePoint+1;i<draft.count;i++)draft.points[i-1]=draft.points[i];draft.count--;draft.points.resize(draft.count);if(draft.count<3)draft.closed=0;
        if(splinePublish(draft)){splinePoint=(std::min)(splinePoint,draft.count-1);say("Spline point deleted. Ctrl+Z: undo.");}return 1;
    }
    EdBuffer<unsigned char> sectors;int owned=0;for(int c=0;c<model->count;c++)owned+=sectors[c]=model->c[c].splineId==splineSelected;
    if(owned){if(!edMapDelete(candidate,model,sectors,1)){say("Cannot delete the last sectors in the level.");return 1;}}else edCopyModel(candidate,model);
    for(int i=0;i<candidate->splineCount;i++)if(candidate->splines[i].id==splineSelected){for(int j=i+1;j<candidate->splineCount;j++)candidate->splines[j-1]=candidate->splines[j];candidate->splineCount--;break;}
    remember();if(commit()){splineSelected=splineDrawing=0;splinePoint=-1;say("Spline and generated sectors deleted. Ctrl+Z: undo.");}return 1;
}
static void splineFinish(void){splineDrawing=splineDragging=0;grabbing=0;say("Spline finished. Select the curve or a point; Enter edits its attributes.");}
static void splineBegin(EdVec position){
    if(netEditPending())return;EdSpline s={};s.id=1;for(int i=0;i<model->splineCount;i++)s.id=(std::max)(s.id,model->splines[i].id+1);
    s.count=1;s.sides=splineLastShape;s.segments=8;s.width=s.height=1;s.material=currentMaterial;s.points[0]={position,edV(0,0,0)};
    if(splinePublish(s)){splineSelected=s.id;splinePoint=0;splineDrawing=1;picks.count=0;sectorPicks.clear();say("Spline: click for points; hold and drag for tangent handles. Escape / RMB finishes.");}
}
static void splineShapeDraw(jkGuiElement *e,jkGuiMenu *menu,tVBuffer *buffer,BOOL redraw){
    if(redraw)jkGuiRend_CopyVBuffer(menu,&e->rect);int sides=(int)e->otherDataPtr;stdDisplay_VBufferLock(buffer);rdCanvas canvas={};canvas.pVBuffer=buffer;canvas.widthMinusOne=buffer->format.width-1;canvas.heightMinusOne=buffer->format.height-1;
    int color=palettePixel(buffer,80,255,240),cx=e->rect.x+e->rect.width/2,cy=e->rect.y+27;double start=sides==4?3.141592653589793/4:3.141592653589793/2;
    for(int i=0;i<sides;i++){double a=start+i*6.283185307179586/sides,b=start+(i+1)*6.283185307179586/sides;rdPrimit2_DrawLine(&canvas,cx+(int)(22*cos(a)),cy-(int)(22*sin(a)),cx+(int)(22*cos(b)),cy-(int)(22*sin(b)),color,-1);}
    char name[48];shapeLabel(sides,name,sizeof(name));if(sides==32)strcpy(name,"Circle (32)");stdFont_DrawAscii(buffer,menu->fonts[0],e->rect.x+2,e->rect.y+58,e->rect.width-4,name,1);stdDisplay_VBufferUnlock(buffer);
}
static void splineMenuTest(jkGuiMenu *menu){
    static int frame,stage;if(++frame%3)return;
    if(getenv("GEOSITH_SPLINE_CONVERT_CAPTURE")){
#ifdef SDL2_RENDER
        std3D_ScreenshotWindow(getenv("GEOSITH_SPLINE_CONVERT_CAPTURE"));
#endif
        menu->lastClicked=22;return;
    }
#ifdef SDL2_RENDER
    const char *path=menu->paElements[1].type==ELEMENT_TEXTBUTTON?getenv("GEOSITH_SPLINE_SHAPE_CAPTURE"):getenv("GEOSITH_SPLINE_MENU_CAPTURE");if(path)std3D_ScreenshotWindow(path);
#endif
    if(menu->paElements[1].type==ELEMENT_TEXTBUTTON)menu->lastClicked=101;
    else {int choices[]={20,21,1};menu->lastClicked=choices[(std::min)(stage++,2)];}
}
static int splineShapeMenu(int sides){
    int page=(sides-3)/10;
    for(;;){jkGuiElement e[15]={};e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,15,600,40},1};
        for(int i=0;i<10;i++){int n=3+page*10+i;e[i+1]={ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20+(i%5)*120,90+(i/5)*115,112,100},n<=32};e[i+1].drawFuncOverride=splineShapeDraw;e[i+1].otherDataPtr=n;}
        e[11]={ELEMENT_TEXTBUTTON,2,2,NULL,3,{20,400,180,40},1};e[12]={ELEMENT_TEXTBUTTON,3,2,NULL,3,{440,400,180,40},1};e[13]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{230,400,180,40},1};e[14].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=u"Spline Cross-section";e[11].wstr=u"Previous";e[12].wstr=u"Next";jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[13]);if((getenv("GEOSITH_SPLINE_MENU_CAPTURE")||getenv("GEOSITH_SPLINE_CONVERT_CAPTURE"))&&getenv("GEOSITH_SPLINE_TEST"))menu.idkFunc=splineMenuTest;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);
        if(choice>=100&&choice<110)return 3+page*10+choice-100;if(choice==2)page=(std::max)(0,page-1);else if(choice==3)page=(std::min)(2,page+1);else return sides;
    }
}
static int splineConvertSelected(void){
    if(netEditPending())return 0;edCopyModel(candidate,model);if(!edSplineConvert(candidate,splineSelected))return 0;
    remember();if(!commit())return 0;splineSelected=splineDrawing=splineDragging=0;splinePoint=-1;say("Spline converted to sectors. Geometry tools are now available. Ctrl+Z restores the spline.");return 1;
}
static void splineAttributes(void){
    int index=splineIndex(splineSelected);if(index<0||netEditPending())return;EdSpline draft=model->splines[index];int point=splinePoint;
    if(point>=draft.count)point=-1;EdModel original;edCopyModel(&original,model);
    int modal=jkMain_bInModalGui;jkMain_bInModalGui=1;sithTime_Pause();stdControl_SetActivation(0);
    if(jkGame_isDDraw){Windows_ShutdownGdi();Video_SwitchToGDI();jkPlayer_Close();jkGame_isDDraw=0;}
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    for(;;){char16_t values[3][128],shape[128],texture[160];char text[128];double numbers[3];
        if(point>=0){numbers[0]=draft.points[point].position.x;numbers[1]=draft.points[point].position.y;numbers[2]=draft.points[point].position.z;}else{numbers[0]=draft.segments;numbers[1]=draft.width;numbers[2]=draft.height;}
        jkGuiElement e[14]={};e[0]={ELEMENT_TEXT,0,2,NULL,3,{20,18,600,40},1};
        for(int i=0;i<3;i++){snprintf(text,sizeof(text),"%.9g",numbers[i]);stdString_CharToWchar(values[i],text,128);e[i+1]={ELEMENT_TEXT,0,0,NULL,3,{30,100+i*60,180,28},1};e[i+4]={ELEMENT_TEXTBOX,10+i,0,NULL,127,{220,98+i*60,380,32},1};}
        e[7]={point<0?ELEMENT_TEXTBUTTON:ELEMENT_TEXT,20,0,NULL,3,{80,290,480,36},1};e[8]={ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,425,230,36},1};e[9]={ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,425,230,36},1};e[10]={ELEMENT_TEXT,0,0,NULL,3,{24,382,592,36},1};e[11]={ELEMENT_TEXTBUTTON,21,0,NULL,3,{40,336,560,36},point<0};e[12]={ELEMENT_TEXTBUTTON,22,0,NULL,3,{40,378,560,32},point<0};e[13].type=ELEMENT_END;
        jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);e[0].wstr=point>=0?u"Spline Point Attributes":u"Spline Shape and Geometry";
        const char16_t *labels[]={u"Sectors (1-256)",u"Width",u"Height",u"X",u"Y",u"Z"};for(int i=0;i<3;i++){e[i+1].wstr=labels[i+(point>=0?3:0)];e[i+4].wstr=values[i];}
        shapeLabel(draft.sides,text,sizeof(text));if(draft.sides==32)strcpy(text,"Circle (32)");stdString_CharToWchar(shape,text,128);e[7].wstr=point>=0?u"Position is the center of the corridor cross-section.":shape;e[8].wstr=u"Apply";e[12].wstr=u"Convert to Sectors";e[10].bIsVisible=point>=0;e[10].wstr=point>=0?u"Move the point without changing its tangent handle.":u"Click the shape to choose a cross-section.\nTight curves may need narrower corridors or more sectors.";
        char textureText[160];snprintf(textureText,sizeof(textureText),"Texture: %s",draft.material>=0&&draft.material<paletteCount?paletteNames[draft.material]:"Choose...");stdString_CharToWchar(texture,textureText,160);e[11].wstr=texture;
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[9]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[8]);if((getenv("GEOSITH_SPLINE_MENU_CAPTURE")||getenv("GEOSITH_SPLINE_CONVERT_CAPTURE"))&&getenv("GEOSITH_SPLINE_TEST"))menu.idkFunc=splineMenuTest;int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(choice<0)break;
        if(choice==22&&point<0){if(edModelsEqual(&original,model)&&splineConvertSelected())break;say("Spline conversion cancelled: level changed.");break;}
        int valid=1;for(int i=0;i<3;i++){char extra;stdString_WcharToChar(text,values[i],128);if(sscanf(text," %lf %c",&numbers[i],&extra)!=1||!isfinite(numbers[i]))valid=0;}
        if(point<0&&(numbers[0]<1||numbers[0]>ED_SPLINE_SECTORS||floor(numbers[0])!=numbers[0]||numbers[1]<ED_GRID||numbers[2]<ED_GRID||numbers[1]>1000||numbers[2]>1000))valid=0;
        if(!valid){jkGuiDialog_ErrorDialog(u"Spline",u"Enter valid numbers. Sector count must be 1-256; width and height must be 0.0625-1000.");continue;}
        if(point>=0)draft.points[point].position=edV(numbers[0],numbers[1],numbers[2]);else{draft.segments=(int)numbers[0];draft.width=numbers[1];draft.height=numbers[2];}
        if(choice==21&&point<0){int oldMaterial=currentMaterial,oldObjects=browserObjects;currentMaterial=draft.material;browserObjects=0;materialMenu(1);draft.material=currentMaterial;currentMaterial=oldMaterial;browserObjects=oldObjects;continue;}
        if(choice==20&&point<0){draft.sides=splineShapeMenu(draft.sides);continue;}
        if(!edModelsEqual(&original,model)){say("Level changed while the spline menu was open. Reopen the menu.");break;}
        if(splinePublish(draft))break;jkGuiDialog_ErrorDialog(u"Spline",u"Cannot generate this corridor. Adjust its shape, dimensions or point positions.");
    }
    jkGui_SetModeGame();jkMain_SetVideoMode();sithTime_Resume();jkMain_bInModalGui=modal;stdControl_SetActivation(1);stdControl_Flush();fireDown=grabDown=1;
}
static int splineControl3D(EdVec origin,EdVec direction,int fire,int grab,int enter){
    if(splineDragging&&!grabbing)splineDragging=0;
    int hitId=0,hitPoint=-1;double best=1000;
    for(int i=0;i<model->splineCount;i++)for(int p=0;p<model->splines[i].count;p++){EdVec offset=edSub(model->splines[i].points[p].position,origin);double distance=edDot(offset,direction);if(distance<=0||distance>=best)continue;double radius=fmin(.12,fmax(.035,distance*.012));EdVec miss=edSub(offset,edScale(direction,distance));if(edDot(miss,miss)<radius*radius){hitId=model->splines[i].id;hitPoint=p;best=distance;}}
    if(fire&&!fireDown&&hitId){splineSelected=hitId;splinePoint=hitPoint;picks.count=0;sectorPicks.clear();fireDown=fire;grabDown=grab;return 1;}
    if(enter&&!splineDragging&&splineIndex(splineSelected)>=0&&splinePoint>=0){splineAttributes();attributesDown=1;return 1;}
    if(grab&&!grabDown&&hitId){splineSelected=hitId;splinePoint=hitPoint;splineDraft=model->splines[splineIndex(hitId)];edCopyModel(snapshot,model);splineDragging=grabbing=1;splineDragStart=splineDraft.points[hitPoint].position;splineDragDelta=edV(0,0,0);edDragTraceBegin(&splineDragTrace,origin,direction,splineDragStart,direction,1000);}
    if(splineDragging){if(grab){splineDragDelta=edAdd(splineDragDelta,edDragTraceStep(&splineDragTrace,origin,direction));splineDraft.points[splinePoint].position=edSnapPoint(edAdd(splineDragStart,splineDragDelta));}
        else {if(edModelsEqual(snapshot,model))splinePublish(splineDraft);splineDragging=grabbing=0;}fireDown=fire;grabDown=grab;return 1;}
    if(fire&&!fireDown&&!hitId){splineSelected=0;splinePoint=-1;}return 0;
}
