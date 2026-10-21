/* Exercise the actual production editor and notifications in native Zune. */
#define main consoleng_preferences_main
#include "../prefs/main.c"
#undef main
#define CHECK(c) do{if(!(c)){printf("PRESETS_FAIL line=%d preset=%lu\n",__LINE__,(unsigned long)index);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void)
{
 int rc=20,opened=0,previewOpened=0;ULONG index=0;struct MsgPort *port=CreateMsgPort();
 struct Window *sample=NULL;Object *app=NULL,*window=NULL,*editor=NULL;
 struct ConsolengRenderProfile before={0},after={0};
 control=port?(void*)CreateIORequest(port,sizeof(*control)):NULL;preview=port?(void*)CreateIORequest(port,sizeof(*preview)):NULL;CHECK(control&&preview);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0));opened=1;
 CHECK(exchange(control,CD_GETRENDERPROFILE,&before,sizeof(before)));
 initial.cursor=before.rp_Theme.tp_Cursor;initial.history=before.rp_Theme.tp_History;initial.font=before.rp_Theme.tp_Font;initial.colors=before.rp_Theme.tp_Colors;initial.emoji=before.rp_Emoji.ep_Enabled;
 sample=OpenWindowTags(NULL,WA_Left,40,WA_Top,380,WA_Width,710,WA_Height,190,WA_Title,(IPTR)"ConsoleNG palette preset preview",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,TAG_DONE);CHECK(sample);
 preview->io_Data=sample;preview->io_Length=sizeof(*sample);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)preview,0));previewOpened=1;
 CHECK(exchange(preview,CD_GETCOLORPREFS,&baselineColors,sizeof(baselineColors)));
 const char *text="\x1b%G\x1b[?25l\r\n  Native ConsoleNG palette presets\r\n\r\n  \x1b[31mRed  \x1b[32mGreen  \x1b[33mYellow  \x1b[34mBlue  \x1b[35mMagenta  \x1b[36mCyan\x1b[39m\r\n\r\n  Test changes this preview only.\r\n  Colors remain editable and survive profile round trips.\r\n";
 CHECK(exchange(preview,CMD_WRITE,(void*)text,strlen(text)));
 app=(Object*)ApplicationObject,MUIA_Application_Title,(IPTR)"ConsoleNG Preferences",MUIA_Application_Base,(IPTR)"PRESETTEST",
 SubWindow,(IPTR)(window=(Object*)SystemPrefsWindowObject,MUIA_Window_LeftEdge,40,MUIA_Window_TopEdge,20,
 WindowContents,(IPTR)(editor=NewObject(ConsoleEditor_CLASS->mcc_Class,NULL,TAG_DONE)),End),End;CHECK(app&&editor);
 SET(window,MUIA_Window_Open,TRUE);CHECK(XGET(window,MUIA_Window_Open));uiReady=1;
 struct ConsoleEditor_DATA *d=INST_DATA(ConsoleEditor_CLASS->mcc_Class,editor);
 SET(d->tabs,MUIA_Group_ActivePage,3);
 for(index=1;index<=5;index++){
  struct ConsolengColorPrefs expected,actual;struct ConsoleSettings selected,decoded;char text[512];
  CHECK(colorPresetApply(&expected,index));SET(d->preset,MUIA_Cycle_Active,index);
  /* Button notification, not direct invocation of the preset handler. */
  SET(d->presetApply,MUIA_Pressed,TRUE);SET(d->presetApply,MUIA_Pressed,FALSE);
  CHECK(collect(d,&selected)&&!memcmp(&selected.colors,&expected,sizeof(expected)));
  CHECK(!memcmp(&selected.cursor,&initial.cursor,sizeof(initial.cursor))&&!memcmp(&selected.font,&initial.font,sizeof(initial.font))&&selected.history.hp_Lines==initial.history.hp_Lines&&selected.emoji==initial.emoji);
  CHECK(settingsFormat(text,sizeof(text),&selected));decoded=initial;CHECK(settingsParse(text,&decoded)&&!memcmp(&selected,&decoded,sizeof(selected)));
  CHECK(DoMethod(editor,MUIM_PrefsEditor_Test));CHECK(exchange(preview,CD_GETCOLORPREFS,&actual,sizeof(actual)));
  CHECK(actual.cp_Mask==expected.cp_Mask);if(index>1)CHECK(!memcmp(actual.cp_RGB,expected.cp_RGB,sizeof(actual.cp_RGB)));
  CHECK(exchange(control,CD_GETRENDERPROFILE,&after,sizeof(after))&&!memcmp(&before,&after,sizeof(before)));
 }
 /* A manual RGB change switches the selector back to custom. */
 SET(d->red,MUIA_Numeric_Value,123);CHECK(XGET(d->preset,MUIA_Cycle_Active)==0&&((d->colors.cp_RGB[0]>>16)&255)==123);
 CHECK(DoMethod(editor,MUIM_PrefsEditor_Revert));CHECK(!memcmp(&d->colors,&initial.colors,sizeof(initial.colors)));
 index=4;SET(d->preset,MUIA_Cycle_Active,index);SET(d->presetApply,MUIA_Pressed,TRUE);SET(d->presetApply,MUIA_Pressed,FALSE);CHECK(DoMethod(editor,MUIM_PrefsEditor_Test));
 puts("PRESETS_PASS five presets, notifications, persistence, isolated Test, custom edit, Revert");fflush(stdout);Delay(600);rc=0;
out:
 if(app)MUI_DisposeObject(app);
 if(previewOpened)CloseDevice((struct IORequest*)preview);
 if(sample)CloseWindow(sample);
 if(opened)CloseDevice((struct IORequest*)control);
 if(preview)DeleteIORequest((struct IORequest*)preview);
 if(control)DeleteIORequest((struct IORequest*)control);
 if(port)DeleteMsgPort(port);
 printf("PRESETS_SCRIPT rc=%d\n",rc);return rc;
}
