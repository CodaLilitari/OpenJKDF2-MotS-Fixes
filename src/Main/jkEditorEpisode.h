/* Episode changes stay in a private draft until Apply. Save Level writes the
 * episode descriptor, current JKL and associated loading strings together. */
static int episodeNumber(const char16_t *text,int *value,int minimum,int maximum){double number;if(!parseHeaderNumbers(text,&number,1)||number!=floor(number)||number<minimum||number>maximum)return 0;*value=(int)number;return 1;}
static void captureEpisodeStrings(jkGuiMenu *menu){
    static int frames,page;if(++frames%3)return;
    if(page==3)stdString_SafeWStrCopy((char16_t*)menu->paElements[12].wstr,u"Final episode intro line",160);
#ifdef SDL2_RENDER
    char path[512];snprintf(path,sizeof(path),"%s.strings%d.png",getenv("GEOSITH_EPISODE_CAPTURE"),page);std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=page++<3?11:1;
}
static void episodeStrings(EdEpisodeRow *row){
    EdEpisodeRow draft=*row;int page=0;
    for(;;){char16_t values[6][160],labels[5][32];stdString_CharToWchar(values[0],draft.title,160);
        for(int i=0;i<5;i++){char text[32];snprintf(text,sizeof(text),"Loading line %d",page*5+i+1);stdString_CharToWchar(labels[i],text,32);stdString_CharToWchar(values[i+1],draft.intro[page*5+i],160);}
        jkGuiElement elements[19]={0};elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};
        elements[1]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,64,150,30},1};elements[2]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,95,{180,62,440,30},1};
        for(int i=0;i<5;i++){elements[3+i]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,112+i*44,150,30},1};elements[8+i]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,159,{180,110+i*44,440,30},1};}
        elements[13]=jkGuiElement{ELEMENT_TEXTBUTTON,10,0,NULL,3,{20,350,180,32},1};elements[14]=jkGuiElement{ELEMENT_TEXTBUTTON,11,0,NULL,3,{440,350,180,32},1};
        elements[15]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,390,600,24},1};elements[16]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};elements[17]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};elements[18].type=ELEMENT_END;
        jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        elements[0].wstr=u"Level Name and Loading Text";elements[1].wstr=u"Level name";elements[2].wstr=values[0];
        for(int i=0;i<5;i++){elements[3+i].wstr=labels[i];elements[8+i].wstr=values[i+1];}
        elements[13].wstr=u"Previous lines";elements[14].wstr=u"Next lines";elements[15].wstr=u"These strings are saved in misc/cogstrings.uni.";elements[16].wstr=u"Done";
        if(getenv("GEOSITH_EPISODE_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureEpisodeStrings;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[16]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[17]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);if(choice<0)return;
        int valid=headerText(draft.title,sizeof(draft.title),values[0]);for(int i=0;i<5;i++)valid=valid&&headerText(draft.intro[page*5+i],160,values[i+1]);
        if(!valid){jkGuiDialog_ErrorDialog(u"Loading Text",u"Text cannot contain double quotes or unsupported characters.");continue;}
        if(choice==1){*row=draft;return;}if(choice==10&&page>0)page--;if(choice==11&&page<3)page++;
    }
}
static void captureEpisodeRow(jkGuiMenu *menu){
    static int frames,stage;if(++frames%3)return;
    stdString_SafeWStrCopy((char16_t*)menu->paElements[12].wstr,u"10",160);stdString_SafeWStrCopy((char16_t*)menu->paElements[19].wstr,u"episode_test.jkl",160);
    stdString_SafeWStrCopy((char16_t*)menu->paElements[21].wstr,u"Episode Level Name",160);stdString_SafeWStrCopy((char16_t*)menu->paElements[23].wstr,u"Episode loading text",160);
#ifdef SDL2_RENDER
    char path[512];snprintf(path,sizeof(path),"%s.row.png",getenv("GEOSITH_EPISODE_CAPTURE"));std3D_ScreenshotWindow(path);
#endif
    menu->lastClicked=stage++?1:201;
}
static void episodeRowDialog(EdEpisode *episode,int index){
    EdEpisodeRow draft=episode->rows[index];int current=episode->current;
    for(;;){
        char16_t values[10][160];int *numbers[]={&draft.line,&draft.cd,&draft.level,&draft.lightPower,&draft.darkPower,&draft.gotoA,&draft.gotoB};
        const char16_t *labels[]={u"Line #",u"CD #",u"Level #",u"Light powers",u"Dark powers",u"Goto A",u"Goto B"};
        for(int i=0;i<7;i++){char text[32];snprintf(text,sizeof(text),"%d",*numbers[i]);stdString_CharToWchar(values[i],text,160);}
        stdString_CharToWchar(values[7],draft.file,160);stdString_CharToWchar(values[8],draft.title,160);stdString_CharToWchar(values[9],draft.intro[0],160);
        jkGuiElement elements[30]={0};elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,10,600,40},1};
        int x[]={20,225,420,20,330,20,330},y[]={70,70,70,160,160,205,205},bx[]={100,290,505,180,490,180,490};
        for(int i=0;i<7;i++){elements[5+i]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{x[i],y[i],bx[i]-x[i]-8,30},1};elements[12+i]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,12,{bx[i],y[i]-2,100,30},1};}
        elements[1]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,115,90,30},1};elements[19]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,31,{115,113,365,30},1};elements[20]=jkGuiElement{ELEMENT_TEXTBUTTON,200,0,NULL,3,{490,113,130,30},1};
        elements[2]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,250,135,30},1};elements[21]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,95,{160,248,460,30},1};
        elements[3]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,295,135,30},1};elements[23]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,159,{160,293,335,30},1};elements[22]=jkGuiElement{ELEMENT_TEXTBUTTON,201,0,NULL,3,{500,293,120,30},1};
        elements[24]=jkGuiElement{ELEMENT_TEXTBUTTON,202,0,NULL,3,{20,342,600,32},1};elements[25]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,385,600,32},1};
        elements[26]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};elements[27]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};elements[28].type=ELEMENT_END;
        jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        elements[0].wstr=u"Episode Sequence Entry";elements[1].wstr=u"File name";elements[2].wstr=u"Level name";elements[3].wstr=u"Loading text";
        for(int i=0;i<7;i++){elements[5+i].wstr=labels[i];elements[12+i].wstr=values[i];}
        elements[19].wstr=values[7];elements[20].wstr=draft.cutscene?u"CUT":u"LEVEL";elements[21].wstr=values[8];elements[22].wstr=u"All text...";elements[23].wstr=values[9];
        elements[24].wstr=index==current?u"This row contains the level currently being edited":u"Use this row for the current level";
        elements[25].wstr=u"Goto -1 ends the episode; other values are sequence line numbers.";elements[26].wstr=u"Update";
        if(getenv("GEOSITH_EPISODE_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureEpisodeRow;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&elements[26]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[27]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(elements);if(choice<0)return;
        int valid=1;for(int i=0;i<7;i++)valid=valid&&episodeNumber(values[i],numbers[i],i>=5?-1:0,i==1?9:i==2?999:i==3||i==4?1000:9999);
        valid=valid&&headerText(draft.file,sizeof(draft.file),values[7])&&draft.file[0]&&headerText(draft.title,sizeof(draft.title),values[8])&&headerText(draft.intro[0],160,values[9]);
        if(!valid){jkGuiDialog_ErrorDialog(u"Episode Entry",u"Enter whole numbers in range and valid text.\nFile names must be at most 31 characters.");continue;}
        if(choice==200){char16_t types[2][128]={u"LEVEL",u"CUT"};int type=chooseList(u"Entry Type",types,2,draft.cutscene);if(type>=0){if(type&&current==index)jkGuiDialog_ErrorDialog(u"Entry Type",u"The current level must use LEVEL. Assign another current row first.");else draft.cutscene=type;}}
        else if(choice==201)episodeStrings(&draft);
        else if(choice==202){if(draft.cutscene)jkGuiDialog_ErrorDialog(u"Current Level",u"Choose a LEVEL entry for the current geometry.");else current=index;}
        else if(choice==1){episode->rows[index]=draft;episode->current=current;return;}
    }
}
static void captureEpisodeMenu(jkGuiMenu *menu){
    static int frames,stage;if(++frames%3)return;
    stdString_SafeWStrCopy((char16_t*)menu->paElements[2].wstr,u"Episode editor test",160);stdString_SafeWStrCopy((char16_t*)menu->paElements[4].wstr,u"3",32);
#ifdef SDL2_RENDER
    std3D_ScreenshotWindow(getenv("GEOSITH_EPISODE_CAPTURE"));
#endif
    menu->lastClicked=stage++?1:100;
}
void jkEditor_EpisodeDialog(void){
    if(!jkEditor_IsLevel()||!bindEditorWorld())return;
    EdEpisode draft=model->episode;if(!draft.count)edEpisodeDefault(&draft,model->header.title,multi);
    snprintf(draft.rows[draft.current].title,sizeof(draft.rows[draft.current].title),"%s",model->header.title);edCopy(draft.rows[draft.current].intro,model->header.intro,sizeof(model->header.intro));
    int page=0,selected=draft.current;
    for(;;){char16_t name[160],type[32],rows[6][160],info[160];stdString_CharToWchar(name,draft.title,160);char text[160];snprintf(text,sizeof(text),"%d",draft.type);stdString_CharToWchar(type,text,32);
        for(int i=0;i<6;i++){int row=page*6+i;if(row<draft.count){EdEpisodeRow *r=&draft.rows[row];snprintf(text,sizeof(text),"%s%4d  %-5s %-31s  %3d  %4d  %4d",row==draft.current?">":" ",r->line,r->cutscene?"CUT":"LEVEL",r->file,r->level,r->gotoA,r->gotoB);stdString_CharToWchar(rows[i],text,160);}else rows[i][0]=0;}
        snprintf(text,sizeof(text),"%d entries | Current JKL: %s | Selected line: %d",draft.count,draft.rows[draft.current].file,draft.rows[selected].line);stdString_CharToWchar(info,text,160);
        jkGuiElement elements[23]={0};elements[0]=jkGuiElement{ELEMENT_TEXT,0,2,NULL,3,{20,8,600,40},1};elements[1]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,54,135,30},1};elements[2]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,95,{160,52,460,30},1};
        elements[3]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,94,135,30},1};elements[4]=jkGuiElement{ELEMENT_TEXTBOX,0,0,NULL,5,{160,92,80,30},1};elements[5]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{260,94,360,30},1};
        elements[6]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,130,600,24},1};
        for(int i=0;i<6;i++)elements[7+i]=jkGuiElement{ELEMENT_TEXTBUTTON,100+i,0,NULL,3,{20,154+i*27,600,26},page*6+i<draft.count};
        for(int i=0;i<6;i++)elements[13+i]=jkGuiElement{ELEMENT_TEXTBUTTON,10+i,0,NULL,3,{20+i*100,326,94,32},1};
        elements[19]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,366,600,25},1};elements[20]=jkGuiElement{ELEMENT_TEXT,0,0,NULL,3,{20,393,600,32},1};
        elements[21]=jkGuiElement{ELEMENT_TEXTBUTTON,1,2,NULL,3,{350,430,230,36},1};elements[22]=jkGuiElement{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,430,230,36},1};
        jkGuiElement all[24];edCopy(all,elements,sizeof(elements));edZero(&all[23],0,sizeof(all[23]));all[23].type=ELEMENT_END;
        jkGuiMenu menu={all,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
        all[0].wstr=u"Episode Editor";all[1].wstr=u"Episode name";all[2].wstr=name;all[3].wstr=u"Episode type";all[4].wstr=type;all[5].wstr=u"1=SP  2=MP  8=CTF; combine flags";all[6].wstr=u"Line   Type   File                             Level   A   B";
        for(int i=0;i<6;i++)all[7+i].wstr=rows[i];const char16_t *buttons[]={u"Previous",u"Next",u"Add",u"Delete",u"Move up",u"Move down"};for(int i=0;i<6;i++)all[13+i].wstr=buttons[i];
        all[19].wstr=info;all[20].wstr=u"Click a row to edit. Apply, then Save Level.\nOther rows reference installed level/cutscene files.";all[21].wstr=u"Apply";
        if(getenv("GEOSITH_EPISODE_CAPTURE")&&getenv("GEOSITH_STRESS")&&!strcmp(jkRes_episodeGobName,"sithed_smoke"))menu.idkFunc=captureEpisodeMenu;
        jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&all[21]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&all[22]);
        int choice=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(all);if(choice<0)return;
        if(!headerText(draft.title,sizeof(draft.title),name)||!draft.title[0]||!episodeNumber(type,&draft.type,1,65535)){jkGuiDialog_ErrorDialog(u"Episode Editor",u"Enter an episode name and a numeric episode type (1-65535).");continue;}
        if(choice>=100&&choice<106){selected=page*6+choice-100;if(selected<draft.count)episodeRowDialog(&draft,selected);}
        else if(choice==10&&page>0)page--;else if(choice==11&&(page+1)*6<draft.count)page++;
        else if(choice==12){if(draft.count>=ED_EPISODE_ROWS)jkGuiDialog_ErrorDialog(u"Episode Editor",u"Maximum 64 sequence entries.");else{int line=0;for(int i=0;i<draft.count;i++)if(draft.rows[i].line>=line)line=draft.rows[i].line+10;selected=draft.count++;draft.rows[selected]=EdEpisodeRow{.line=line,.cd=1,.level=selected+1,.gotoA=-1,.gotoB=-1};snprintf(draft.rows[selected].file,32,"level%d.jkl",selected+1);page=selected/6;episodeRowDialog(&draft,selected);}}
        else if(choice==13){if(selected==draft.current)jkGuiDialog_ErrorDialog(u"Episode Editor",u"Assign the current level to another LEVEL row before deleting this row.");else{memmove(&draft.rows[selected],&draft.rows[selected+1],sizeof(EdEpisodeRow)*(draft.count-selected-1));edZero(&draft.rows[--draft.count],0,sizeof(EdEpisodeRow));if(draft.current>selected)draft.current--;if(selected>=draft.count)selected=draft.count-1;if(page*6>=draft.count)page=selected/6;}}
        else if(choice==14||choice==15){int other=selected+(choice==14?-1:1);if(other>=0&&other<draft.count){EdEpisodeRow swap=draft.rows[selected];draft.rows[selected]=draft.rows[other];draft.rows[other]=swap;if(draft.current==selected)draft.current=other;else if(draft.current==other)draft.current=selected;selected=other;page=selected/6;}}
        else if(choice==1){if(!edEpisodeValidate(&draft)){jkGuiDialog_ErrorDialog(u"Episode Editor",u"Use unique line numbers and valid filenames (LEVEL requires .jkl).\nGoto A/B must be -1 or an existing line number. The current row must be LEVEL.");continue;}
            remember();edCopyModel(candidate,model);candidate->episode=draft;EdEpisodeRow *r=&draft.rows[draft.current];if(r->title[0])snprintf(candidate->header.title,sizeof(candidate->header.title),"%s",r->title);edCopy(candidate->header.intro,r->intro,sizeof(candidate->header.intro));
            if(commit()){say("Episode updated. Use Save Level to write episode.jk to the GOB.");return;}
        }
    }
}
