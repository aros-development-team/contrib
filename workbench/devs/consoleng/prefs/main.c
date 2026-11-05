/* ConsoleNG native Zune editor. Shared defaults today; no invented user API. */
#define MUIMASTER_YES_INLINE_STDARG
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/alib.h>
#include <proto/asl.h>
#include <libraries/asl.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <zune/customclasses.h>
#include <zune/prefseditor.h>
#include <zune/systemprefswindow.h>
#include <zune/prefswindow.h>
#define MUIM_ConsoleBindingSelect (TAG_USER | 0x2100000a)
#define MUIM_ConsoleBindingEdit (TAG_USER | 0x2100000b)
#define MUIM_ConsoleCursorColor (TAG_USER | 0x21000009)
#define MUIM_ConsoleColorPreset (TAG_USER | 0x21000008)
#define MUIM_ConsoleColorSelect (TAG_USER | 0x21000005)
#define MUIM_ConsoleColorEdit (TAG_USER | 0x21000006)
#define MUIM_ConsoleColorInherit (TAG_USER | 0x21000007)
#define MUIM_ConsoleFont (TAG_USER | 0x21000003)
#define MUIM_ConsoleInherit (TAG_USER | 0x21000004)
#define MUIM_ConsoleLoad (TAG_USER | 0x21000001)
#define MUIM_ConsoleWrite (TAG_USER | 0x21000002)
#include <devices/conunit.h>
#include <stdlib.h>
#include "consoleng.h"
#include "settings.h"
#include "options.h"
#include "presets.h"
#define ENVFILE "ENV:SYS/consoleng.prefs"
#define ARCFILE "ENVARC:SYS/consoleng.prefs"
static struct IOStdReq *control,*preview;
static struct ConsoleSettings initial;
static struct ConsolengColorPrefs baselineColors;
static struct PrefsOptions options;
static int setupComplete,uiReady;
struct ConsoleEditor_DATA {Object *style,*interval,*timeout,*history,*wheel,*semantic,*fontname,*pixels,*choose,*inherit,*load,*write;
 Object *tabs,*preset,*presetApply,*colorSlot,*red,*green,*blue,*swatch,*colorInherit,*emoji,*bell,*oscCopy,*oscRead;
 Object *bindingSlot,*bindingScreen,*bindingCursor,*bindingKeypad,*bindingSearch,*bindingMode,*bindingAction,*bindingKey,*bindingSpecial,*bindingMods[5];
 struct ConsolengKeyBinding bindingRows[16];ULONG bindingIndex;
 Object *cursorCustom,*cursorRed,*cursorGreen,*cursorBlue,*cursorSwatch;
 struct ConsolengColorPrefs colors;ULONG colorIndex;int populating;};
static const char *styles[]={"Default block","Blinking block","Steady block","Blinking underline","Steady underline","Blinking bar","Steady bar",NULL};
static const char *pages[]={"Cursor","Scrollback","Font","Colors","Bell","Clipboard","Keyboard","Selection",NULL};
static const char *colorNames[]={"Foreground","Background","Black","Red","Green","Yellow","Blue","Magenta","Cyan","White","Bright black","Bright red","Bright green","Bright yellow","Bright blue","Bright magenta","Bright cyan","Bright white",NULL};
static const char *bindingSlots[]={"Shortcut 1","Shortcut 2","Shortcut 3","Shortcut 4","Shortcut 5","Shortcut 6","Shortcut 7","Shortcut 8","Shortcut 9","Shortcut 10","Shortcut 11","Shortcut 12","Shortcut 13","Shortcut 14","Shortcut 15","Shortcut 16",NULL};
static const char *bindingScreens[]={"Any screen","Alternate screen","Primary screen",NULL};
static const char *bindingApplications[]={"Any mode","Application mode","Normal mode",NULL};
static const char *bindingModes[]={"Always","Navigation","Outside navigation",NULL};
static const char *bindingActions[]={"Disabled","Copy","Paste","Select all","Search","History line up","History line down","History page up","History page down","History top","History bottom","Search backward","Navigate retained text","Clear scrollback","Larger font","Smaller font","Reset font zoom","History half page up","History half page down","Clear selection","Center navigation cursor","Open navigation link","Select navigation characters","Select navigation lines","Select navigation rectangle","Select navigation words","Navigate: Left","Navigate: Right","Navigate: Up","Navigate: Down","Navigate: First column","Navigate: Line end","Navigate: History start","Navigate: History end","Navigate: Next whitespace word","Navigate: Whitespace word end","Navigate: Previous whitespace word","Navigate: Next semantic word","Navigate: Semantic word end","Navigate: Previous semantic word","Navigate: Previous paragraph","Navigate: Next paragraph","Navigate: Matching bracket","Navigate: First occupied cell","Navigate: View top","Navigate: View middle","Navigate: View bottom","Navigate: Page up","Navigate: Page down","Navigate: Half page up","Navigate: Half page down","Navigate: Scroll line up","Navigate: Scroll line down","Navigate: Find forward","Navigate: Find backward","Navigate: Till forward","Navigate: Till backward","Navigate: Repeat find","Navigate: Reverse find","Navigate: Search forward","Navigate: Search backward","Navigate: Search word forward","Navigate: Search word backward","Navigate: Next match","Navigate: Previous match",NULL};
static const char *bindingSearchModes[]={"Any","Searching","Not searching",NULL};
static const char *specialNames[]={"Character","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12","Up","Down","Left","Right","Home","End","Page up","Page down","Insert","Delete","Escape","Tab","Backspace","Return","Keypad Enter","Help","Physical code (hex)",NULL};
static const ULONG specialCodes[]={0,CONSOLENG_BIND_RAW+0x50,CONSOLENG_BIND_RAW+0x51,CONSOLENG_BIND_RAW+0x52,CONSOLENG_BIND_RAW+0x53,CONSOLENG_BIND_RAW+0x54,CONSOLENG_BIND_RAW+0x55,CONSOLENG_BIND_RAW+0x56,CONSOLENG_BIND_RAW+0x57,CONSOLENG_BIND_RAW+0x58,CONSOLENG_BIND_RAW+0x59,CONSOLENG_BIND_RAW+0x4b,CONSOLENG_BIND_RAW+0x6f,CONSOLENG_BIND_RAW+0x4c,CONSOLENG_BIND_RAW+0x4d,CONSOLENG_BIND_RAW+0x4f,CONSOLENG_BIND_RAW+0x4e,CONSOLENG_BIND_RAW+0x70,CONSOLENG_BIND_RAW+0x71,CONSOLENG_BIND_RAW+0x48,CONSOLENG_BIND_RAW+0x49,CONSOLENG_BIND_RAW+0x47,CONSOLENG_BIND_RAW+0x46,CONSOLENG_BIND_RAW+0x45,CONSOLENG_BIND_RAW+0x42,CONSOLENG_BIND_RAW+0x41,CONSOLENG_BIND_RAW+0x44,CONSOLENG_BIND_RAW+0x43,CONSOLENG_BIND_RAW+0x5f};
static void showBinding(struct ConsoleEditor_DATA *d)
{
    struct ConsolengKeyBinding *b=&d->bindingRows[d->bindingIndex];char key[3]={b->kb_Key>255?0:(char)b->kb_Key,0,0};
    ULONG special=0;while(special+1<sizeof(specialCodes)/sizeof(*specialCodes)&&specialCodes[special]!=b->kb_Key)special++;
    if(specialCodes[special]!=b->kb_Key)special=0;
    if(!special && con_binding_physical(b->kb_Key)){special=sizeof(specialCodes)/sizeof(*specialCodes);snprintf(key,sizeof(key),"%02lx",(unsigned long)(b->kb_Key&127));}
    d->populating=1;
    NNSET(d->bindingSpecial,MUIA_Cycle_Active,special);
    NNSET(d->bindingKey,MUIA_Disabled,special!=0 && special<sizeof(specialCodes)/sizeof(*specialCodes));
    NNSET(d->bindingAction,MUIA_Cycle_Active,b->kb_Action);
    NNSET(d->bindingMode,MUIA_Cycle_Active,(b->kb_Modifiers&CONSOLENG_BIND_NAVONLY)?1:(b->kb_Modifiers&CONSOLENG_BIND_NOTNAV)?2:0);
    NNSET(d->bindingScreen,MUIA_Cycle_Active,(b->kb_Modifiers&CONSOLENG_BIND_ALTSCREEN)?1:(b->kb_Modifiers&CONSOLENG_BIND_PRIMARY)?2:0);
    NNSET(d->bindingCursor,MUIA_Cycle_Active,(b->kb_Modifiers&CONSOLENG_BIND_APPCURSOR)?1:(b->kb_Modifiers&CONSOLENG_BIND_NORMALCURSOR)?2:0);
    NNSET(d->bindingSearch,MUIA_Cycle_Active,(b->kb_Modifiers&CONSOLENG_BIND_SEARCHMODE)?1:(b->kb_Modifiers&CONSOLENG_BIND_NOTSEARCH)?2:0);
    NNSET(d->bindingKeypad,MUIA_Cycle_Active,(b->kb_Modifiers&CONSOLENG_BIND_APPKEYPAD)?1:(b->kb_Modifiers&CONSOLENG_BIND_NORMALKEYPAD)?2:0);
    NNSET(d->bindingKey,MUIA_String_Contents,(IPTR)key);
    for(unsigned i=0;i<5;i++)NNSET(d->bindingMods[i],MUIA_Selected,!!(b->kb_Modifiers&(1U<<i)));
    d->populating=0;
}
static void showColor(struct ConsoleEditor_DATA *d)
{
    ULONG i=d->colorIndex;
    int custom=!!(d->colors.cp_Mask&(1UL<<i));
    ULONG rgb=custom?d->colors.cp_RGB[i]:baselineColors.cp_RGB[i];
    ULONG components[3]={((rgb>>16)&255)*0x01010101UL,((rgb>>8)&255)*0x01010101UL,(rgb&255)*0x01010101UL};
    d->populating=1;
    NNSET(d->red,MUIA_Numeric_Value,(rgb>>16)&255);NNSET(d->green,MUIA_Numeric_Value,(rgb>>8)&255);NNSET(d->blue,MUIA_Numeric_Value,rgb&255);
    NNSET(d->colorInherit,MUIA_Selected,!custom);
    /* Colorfield on this Zune baseline accesses the RastPort during OM_SET.
       Populate before Window_Open must not trigger that drawing path. */
    if(uiReady)NNSET(d->swatch,MUIA_Colorfield_RGB,(IPTR)components);
    d->populating=0;
}
static void showCursorColor(struct ConsoleEditor_DATA *d)
{
    ULONG c[3]={XGET(d->cursorRed,MUIA_Numeric_Value)*0x01010101UL,
        XGET(d->cursorGreen,MUIA_Numeric_Value)*0x01010101UL,
        XGET(d->cursorBlue,MUIA_Numeric_Value)*0x01010101UL};
    if(uiReady)NNSET(d->cursorSwatch,MUIA_Colorfield_RGB,(IPTR)c);
}
static void notice(Object *self,const char *s)
{
    MUI_Request(_app(self),_win(self),0,(CONST_STRPTR)"ConsoleNG Preferences",(CONST_STRPTR)"OK",(CONST_STRPTR)"%s",s);
}
static int exchange(struct IOStdReq *io,UWORD cmd,void *p,ULONG size)
{
    io->io_Command=cmd;io->io_Data=p;io->io_Length=size;
    return !DoIO((struct IORequest*)io)&&io->io_Actual==size;
}
static int validFont(const struct ConsoleSettings *p)
{
    return settingsValid(p)&&exchange(control,CD_VALIDATEFONTPREFS,(void*)&p->font,sizeof(p->font));
}
static int apply(struct IOStdReq *io,int defaults,const struct ConsoleSettings *p)
{
    if(!settingsValid(p))return 0;
    struct ConsolengSemanticPrefs semantic=p->semantic.sp_Size?p->semantic:con_semantic_defaults();
    if(defaults) {
        struct ConsolengAppearanceProfile all={sizeof(all),1,
            {sizeof(all.rp_Theme),1,p->cursor,p->history,p->font,p->colors},
            {sizeof(all.rp_Emoji),1,p->emoji},{sizeof(all.rp_Bell),1,p->bell},{sizeof(all.rp_Clipboard),(p->oscCopy&2)?2:1,p->oscCopy},{sizeof(all.rp_CursorColor),1,p->cursorCustom,p->cursorRGB}};
        struct ConsolengInteractionProfile full={sizeof(full),1,all,p->bindings};
        struct ConsolengInputProfile input={sizeof(input),1,full,{sizeof(input.ip_Scroll),1,p->wheelLines}};
        struct ConsolengSelectionProfile selection={sizeof(selection),1,input,semantic};
        return exchange(io,CD_SETSELECTIONPROFILE,&selection,sizeof(selection));
    }
    struct ConsoleSettings old;
    struct ConsolengScrollPrefs oldScroll,scroll={sizeof(scroll),1,p->wheelLines};
    struct ConsolengCursorColorPrefs oldColor,cursorColor={sizeof(cursorColor),1,p->cursorCustom,p->cursorRGB};
    struct ConsolengEmojiPrefs oldEmoji,emoji={sizeof(emoji),1,p->emoji};
    struct ConsolengBellPrefs oldBell,bell={sizeof(bell),1,p->bell};
    struct ConsolengClipboardPrefs oldCopy,copy={sizeof(copy),(p->oscCopy&2)?2:1,p->oscCopy};
    if(!exchange(io,CD_GETCURSORPREFS,&old.cursor,sizeof(old.cursor))||
       !exchange(io,CD_GETHISTORYPREFS,&old.history,sizeof(old.history))||
       !exchange(io,CD_GETFONTPREFS,&old.font,sizeof(old.font))||
       !exchange(io,CD_GETCOLORPREFS,&old.colors,sizeof(old.colors))||
       !exchange(io,CD_GETEMOJIPREFS,&oldEmoji,sizeof(oldEmoji))||
       !exchange(io,CD_GETBELLPREFS,&oldBell,sizeof(oldBell))||
       !exchange(io,CD_GETCLIPBOARDPREFS,&oldCopy,sizeof(oldCopy))||
       !exchange(io,CD_GETCURSORCOLORPREFS,&oldColor,sizeof(oldColor))||
       !exchange(io,CD_GETSCROLLPREFS,&oldScroll,sizeof(oldScroll))||
       !exchange(io,CD_GETSEMANTICPREFS,&old.semantic,sizeof(old.semantic))||
       !exchange(io,CD_GETBINDINGPREFS,&old.bindings,sizeof(old.bindings)))return 0;
    if(!exchange(io,CD_SETFONTPREFS,(void*)&p->font,sizeof(p->font)))return 0;
    if(!exchange(io,CD_SETCOLORPREFS,(void*)&p->colors,sizeof(p->colors))){exchange(io,CD_SETFONTPREFS,&old.font,sizeof(old.font));return 0;}
    if(exchange(io,CD_SETCURSORPREFS,(void*)&p->cursor,sizeof(p->cursor))&&
       exchange(io,CD_SETHISTORYPREFS,(void*)&p->history,sizeof(p->history))&&
       exchange(io,CD_SETEMOJIPREFS,&emoji,sizeof(emoji))&&
       exchange(io,CD_SETBELLPREFS,&bell,sizeof(bell))&&
       exchange(io,CD_SETCLIPBOARDPREFS,&copy,sizeof(copy))&&
       exchange(io,CD_SETCURSORCOLORPREFS,&cursorColor,sizeof(cursorColor))&&
       exchange(io,CD_SETBINDINGPREFS,(void*)&p->bindings,sizeof(p->bindings))&&
       exchange(io,CD_SETSCROLLPREFS,&scroll,sizeof(scroll))&&
       exchange(io,CD_SETSEMANTICPREFS,&semantic,sizeof(semantic)))return 1;
    exchange(io,CD_SETFONTPREFS,&old.font,sizeof(old.font));
    exchange(io,CD_SETCOLORPREFS,&old.colors,sizeof(old.colors));
    exchange(io,CD_SETCURSORPREFS,&old.cursor,sizeof(old.cursor));
    exchange(io,CD_SETHISTORYPREFS,&old.history,sizeof(old.history));
    exchange(io,CD_SETEMOJIPREFS,&oldEmoji,sizeof(oldEmoji));
    exchange(io,CD_SETBELLPREFS,&oldBell,sizeof(oldBell));
    exchange(io,CD_SETCLIPBOARDPREFS,&oldCopy,sizeof(oldCopy));
    exchange(io,CD_SETCURSORCOLORPREFS,&oldColor,sizeof(oldColor));
    exchange(io,CD_SETBINDINGPREFS,&old.bindings,sizeof(old.bindings));
    exchange(io,CD_SETSCROLLPREFS,&oldScroll,sizeof(oldScroll));
    exchange(io,CD_SETSEMANTICPREFS,&old.semantic,sizeof(old.semantic));
    return 0;
}
static void populate(struct ConsoleEditor_DATA *d,const struct ConsoleSettings *p)
{
    memcpy(d->bindingRows,p->bindings.bp_Entries,sizeof(d->bindingRows));
    d->bindingIndex=0;NNSET(d->bindingSlot,MUIA_Cycle_Active,0);showBinding(d);
    NNSET(d->cursorCustom,MUIA_Selected,p->cursorCustom);
    NNSET(d->cursorRed,MUIA_Numeric_Value,(p->cursorRGB>>16)&255);
    NNSET(d->cursorGreen,MUIA_Numeric_Value,(p->cursorRGB>>8)&255);
    NNSET(d->cursorBlue,MUIA_Numeric_Value,p->cursorRGB&255);showCursorColor(d);
    d->colors=p->colors;showColor(d);NNSET(d->preset,MUIA_Cycle_Active,0);
    NNSET(d->emoji,MUIA_Selected,p->emoji);
    NNSET(d->bell,MUIA_Selected,p->bell);
    NNSET(d->oscCopy,MUIA_Selected,p->oscCopy&1);
    NNSET(d->oscRead,MUIA_Selected,!!(p->oscCopy&2));
    NNSET(d->style,MUIA_Cycle_Active,p->cursor.cp_Style);
    NNSET(d->interval,MUIA_String_Integer,p->cursor.cp_BlinkIntervalMS);
    NNSET(d->timeout,MUIA_String_Integer,p->cursor.cp_IdleTimeoutMS);
    NNSET(d->history,MUIA_String_Integer,p->history.hp_Lines);
    NNSET(d->wheel,MUIA_String_Integer,p->wheelLines);
    char separators[SEMANTIC_TEXT_CAP];
    struct ConsolengSemanticPrefs semantic=p->semantic.sp_Size?p->semantic:con_semantic_defaults();
    if(semanticTextFormat(separators,sizeof(separators),&semantic))NNSET(d->semantic,MUIA_String_Contents,(IPTR)separators);
    NNSET(d->fontname,MUIA_String_Contents,(IPTR)p->font.fp_Name);
    NNSET(d->pixels,MUIA_String_Integer,p->font.fp_Pixels);
}
static int number(Object *o,ULONG *out)
{
    const unsigned char *s=(void*)XGET(o,MUIA_String_Contents);ULONG n=0;
    if(!s||!*s)return 0;
    for(;*s;s++){if(*s<'0'||*s>'9'||n>(0xffffffffUL-(*s-'0'))/10)return 0;n=n*10+*s-'0';}
    *out=n;return 1;
}
static int collect(struct ConsoleEditor_DATA *d,struct ConsoleSettings *p)
{
    *p=initial;p->colors=d->colors;
    memset(&p->bindings,0,sizeof(p->bindings));p->bindings.bp_Size=sizeof(p->bindings);p->bindings.bp_Version=1;
    for(unsigned i=0;i<16;i++)if(d->bindingRows[i].kb_Action) {
        p->bindings.bp_Entries[p->bindings.bp_Count++]=d->bindingRows[i];
        if(d->bindingRows[i].kb_Key>255 && p->bindings.bp_Version<2)p->bindings.bp_Version=2;
        if(d->bindingRows[i].kb_Action>CONSOLENG_BIND_SEARCH && p->bindings.bp_Version<3)p->bindings.bp_Version=3;
        if(d->bindingRows[i].kb_Action==CONSOLENG_BIND_SEARCH_BACKWARD && p->bindings.bp_Version<4)p->bindings.bp_Version=4;
        if(con_binding_physical(d->bindingRows[i].kb_Key)&&!con_binding_special(d->bindingRows[i].kb_Key) && p->bindings.bp_Version<5)p->bindings.bp_Version=5;
    }
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action==CONSOLENG_BIND_NAVIGATE)p->bindings.bp_Version=6;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action==CONSOLENG_BIND_CLEARHISTORY)p->bindings.bp_Version=7;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_FONTLARGER)p->bindings.bp_Version=8;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_HALFUP)p->bindings.bp_Version=9;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action==CONSOLENG_BIND_CLEARSELECTION)p->bindings.bp_Version=10;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_NAVCENTER)p->bindings.bp_Version=11;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Modifiers&CONSOLENG_BIND_MODEMASK)p->bindings.bp_Version=12;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_NAVSELECTCHAR)p->bindings.bp_Version=13;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_NAVLEFT)p->bindings.bp_Version=14;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_NAVVIEWTOP)p->bindings.bp_Version=15;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_NAVFINDFORWARD)p->bindings.bp_Version=16;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Action>=CONSOLENG_BIND_NAVSEARCHFORWARD)p->bindings.bp_Version=17;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Modifiers&(CONSOLENG_BIND_CONTEXTMASK & ~CONSOLENG_BIND_MODEMASK))p->bindings.bp_Version=18;
    for(unsigned i=0;i<p->bindings.bp_Count;i++)if(p->bindings.bp_Entries[i].kb_Modifiers&(CONSOLENG_BIND_SEARCHMODE|CONSOLENG_BIND_NOTSEARCH))p->bindings.bp_Version=19;
    p->cursorCustom=!!XGET(d->cursorCustom,MUIA_Selected);
    p->cursorRGB=(XGET(d->cursorRed,MUIA_Numeric_Value)<<16)|
        (XGET(d->cursorGreen,MUIA_Numeric_Value)<<8)|XGET(d->cursorBlue,MUIA_Numeric_Value);
    p->emoji=!!XGET(d->emoji,MUIA_Selected);
    p->bell=!!XGET(d->bell,MUIA_Selected);
    p->oscCopy=!!XGET(d->oscCopy,MUIA_Selected)|(XGET(d->oscRead,MUIA_Selected)?2:0);
    const char *name=(const char*)XGET(d->fontname,MUIA_String_Contents);
    if(!name||strlen(name)>=sizeof(p->font.fp_Name))return 0;
    strcpy(p->font.fp_Name,name);
    if(!number(d->pixels,&p->font.fp_Pixels))return 0;
    if(!semanticTextParse((const char*)XGET(d->semantic,MUIA_String_Contents),&p->semantic))return 0;
    p->cursor.cp_Style=XGET(d->style,MUIA_Cycle_Active);
    return number(d->interval,&p->cursor.cp_BlinkIntervalMS)&&number(d->timeout,&p->cursor.cp_IdleTimeoutMS)&&
        number(d->history,&p->history.hp_Lines)&&number(d->wheel,&p->wheelLines)&&settingsValid(p);
}
static Object *integer(const char *help)
{
    return (Object*)StringObject,StringFrame,MUIA_String_Accept,(IPTR)"0123456789",
        MUIA_String_MaxLen,11,MUIA_CycleChain,TRUE,MUIA_ShortHelp,(IPTR)help,End;
}
static Object *bindingToggle(void)
{
    return (Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,
        MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End;
}
Object *ConsoleEditor__OM_NEW(Class *cl,Object *self,struct opSet *msg)
{
    self=(Object*)DoSuperNewTags(cl,self,NULL,MUIA_PrefsEditor_Name,(IPTR)"ConsoleNG Preferences",
        MUIA_PrefsEditor_Path,(IPTR)"SYS/consoleng.prefs",MUIA_PrefsEditor_IconTool,(IPTR)"SYS:Prefs/ConsoleNG",TAG_DONE);
    if(!self)return NULL;
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);
    ULONG swatchRGB=(initial.colors.cp_Mask&1)?initial.colors.cp_RGB[0]:baselineColors.cp_RGB[0];
    Object *content=(Object*)VGroup,
        Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Console defaults for new sessions",End,
        Child,(IPTR)(d->tabs=(Object*)RegisterGroup(pages),
            Child,(IPTR)VGroup,
                Child,(IPTR)ColGroup(2),
                    Child,(IPTR)Label1("Appearance"),Child,(IPTR)(d->style=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)styles,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Blink interval (ms)"),Child,(IPTR)(d->interval=integer("50 to 5000 milliseconds per blink phase.")),
                    Child,(IPTR)Label1("Idle timeout (ms)"),Child,(IPTR)(d->timeout=integer("Zero keeps blinking. Otherwise 1000 to 3600000 milliseconds.")),
                End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Idle timeout: 0 means keep blinking.",End,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->cursorCustom=(Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Custom cursor color"),End,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->cursorSwatch=(Object*)ColorfieldObject,MUIA_FixWidth,64,End),
                    Child,(IPTR)ColGroup(2),
                        Child,(IPTR)Label1("Red"),Child,(IPTR)(d->cursorRed=(Object*)SliderObject,MUIA_Numeric_Min,0,MUIA_Numeric_Max,255,MUIA_CycleChain,TRUE,End),
                        Child,(IPTR)Label1("Green"),Child,(IPTR)(d->cursorGreen=(Object*)SliderObject,MUIA_Numeric_Min,0,MUIA_Numeric_Max,255,MUIA_CycleChain,TRUE,End),
                        Child,(IPTR)Label1("Blue"),Child,(IPTR)(d->cursorBlue=(Object*)SliderObject,MUIA_Numeric_Min,0,MUIA_Numeric_Max,255,MUIA_CycleChain,TRUE,End),
                    End,
                End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Unchecked uses the traditional inverted cursor.\nIndexed screens use the closest color.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)ColGroup(2),Child,(IPTR)Label1("History lines"),Child,(IPTR)(d->history=integer("Lines retained above the visible screen. Zero disables history. Memory grows with actual output.")),End,
                Child,(IPTR)ColGroup(2),Child,(IPTR)Label1("Wheel lines"),Child,(IPTR)(d->wheel=integer("Lines per wheel event: 0 disables local scrolling, 1..100 sets distance. Also controls alternate-screen arrow repetition; application mouse reporting stays independent.")),End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"0 keeps only the visible screen.\nMemory is allocated as output arrives.\nAlternate-screen applications keep no history.\nWheel: 0 disables; 1..100 lines per event.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)ColGroup(2),
                    Child,(IPTR)Label1("Font name"),Child,(IPTR)(d->fontname=(Object*)StringObject,StringFrame,MUIA_String_MaxLen,128,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Pixel height"),Child,(IPTR)(d->pixels=integer("Exact pixel height of an installed fixed-width font. Zero with an empty name inherits the window font.")),
                End,
                Child,(IPTR)HGroup,Child,(IPTR)(d->choose=SimpleButton("Choose _font")),Child,(IPTR)(d->inherit=SimpleButton("_Window default")),End,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->emoji=(Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Color emoji fallback"),End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Use Noto Color Emoji for missing characters.\nRequires the installed Noto Color Emoji font package.",End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Use an installed fixed-width font, such as DejaVuSansMono.font.\nAn empty name and height 0 inherit the window font.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->preset=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)colorPresetNames,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)(d->presetApply=SimpleButton("Apply _preset")),
                End,
                Child,(IPTR)(d->colorSlot=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)colorNames,MUIA_CycleChain,TRUE,End),
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->swatch=(Object*)ColorfieldObject,MUIA_FixWidth,64,MUIA_Colorfield_Red,((swatchRGB>>16)&255)*0x01010101UL,MUIA_Colorfield_Green,((swatchRGB>>8)&255)*0x01010101UL,MUIA_Colorfield_Blue,(swatchRGB&255)*0x01010101UL,End),
                    Child,(IPTR)ColGroup(2),
                        Child,(IPTR)Label1("Red"),Child,(IPTR)(d->red=(Object*)SliderObject,MUIA_Numeric_Min,0,MUIA_Numeric_Max,255,MUIA_CycleChain,TRUE,End),
                        Child,(IPTR)Label1("Green"),Child,(IPTR)(d->green=(Object*)SliderObject,MUIA_Numeric_Min,0,MUIA_Numeric_Max,255,MUIA_CycleChain,TRUE,End),
                        Child,(IPTR)Label1("Blue"),Child,(IPTR)(d->blue=(Object*)SliderObject,MUIA_Numeric_Min,0,MUIA_Numeric_Max,255,MUIA_CycleChain,TRUE,End),
                    End,
                End,
                Child,(IPTR)HGroup,Child,(IPTR)(d->colorInherit=(Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End),Child,(IPTR)Label1("Inherit this color"),End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"RGB: 0 to 255. Moving a slider overrides this color.\nTest previews the palette; Revert restores the original.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->bell=(Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Enable native visual bell"),
                End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"BEL flashes the AROS screen bars. No sound is played.\nTest previews this setting. Use/Save affect new sessions.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->oscCopy=(Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Allow applications to copy to the clipboard"),
                End,
                Child,(IPTR)HGroup,
                    Child,(IPTR)(d->oscRead=(Object*)ImageObject,ImageButtonFrame,MUIA_InputMode,MUIV_InputMode_Toggle,MUIA_Image_Spec,MUII_CheckMark,MUIA_Image_FreeVert,TRUE,MUIA_ShowSelState,FALSE,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Allow applications to read the clipboard"),
                End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Applies to terminal clipboard requests (OSC 52).\nRead access is off by default; enable only for trusted applications.\nKeyboard and mouse copy remain available.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)(d->bindingSlot=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingSlots,MUIA_CycleChain,TRUE,End),
                Child,(IPTR)ColGroup(2),
                    Child,(IPTR)Label1("Action"),Child,(IPTR)(d->bindingAction=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingActions,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Active in"),Child,(IPTR)(d->bindingMode=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingModes,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Screen"),Child,(IPTR)(d->bindingScreen=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingScreens,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Cursor keys"),Child,(IPTR)(d->bindingCursor=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingApplications,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Keypad"),Child,(IPTR)(d->bindingKeypad=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingApplications,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Search"),Child,(IPTR)(d->bindingSearch=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)bindingSearchModes,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Key type"),Child,(IPTR)(d->bindingSpecial=(Object*)CycleObject,MUIA_Cycle_Entries,(IPTR)specialNames,MUIA_CycleChain,TRUE,End),
                    Child,(IPTR)Label1("Character / hex"),Child,(IPTR)(d->bindingKey=(Object*)StringObject,StringFrame,MUIA_String_MaxLen,3,MUIA_CycleChain,TRUE,End),
                End,
                Child,(IPTR)ColGroup(2),
                    Child,(IPTR)Label1("Shift"),Child,(IPTR)(d->bindingMods[0]=bindingToggle()),
                    Child,(IPTR)Label1("Alt"),Child,(IPTR)(d->bindingMods[1]=bindingToggle()),
                    Child,(IPTR)Label1("Ctrl"),Child,(IPTR)(d->bindingMods[2]=bindingToggle()),
                    Child,(IPTR)Label1("Left Amiga"),Child,(IPTR)(d->bindingMods[3]=bindingToggle()),
                    Child,(IPTR)Label1("Right Amiga"),Child,(IPTR)(d->bindingMods[4]=bindingToggle()),
                End,
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Character: one unshifted character. Physical: two hex digits.\nDisabled removes the shortcut. Active modes must not overlap.\nPhysical and named keys override character shortcuts.",End,
            End,
            Child,(IPTR)VGroup,
                Child,(IPTR)Label1("Word separators"),
                Child,(IPTR)(d->semantic=(Object*)StringObject,StringFrame,MUIA_String_MaxLen,SEMANTIC_TEXT_CAP,MUIA_CycleChain,TRUE,
                    MUIA_ShortHelp,(IPTR)"Up to 64 unique characters. Use \\t for Tab, \\u2502 for Unicode, or \\U0001f600 for a supplementary character. Empty means no explicit separators.",End),
                Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Controls semantic navigation, word selection and seeded search.\nUse ordinary characters or Unicode escapes; each character once.\nNew navigation sessions use the new setting.\nExisting scans keep their original boundaries.",End,
            End,
        End),
        Child,(IPTR)HGroup,
            Child,(IPTR)(d->load=SimpleButton("_Load profile")),
            Child,(IPTR)(d->write=SimpleButton("_Write profile")),
        End,
        Child,(IPTR)TextObject,MUIA_Text_Contents,(IPTR)"Test changes only the preview. Use/Save affect new consoles.\nScope: shared device defaults; existing sessions stay unchanged.",End,
    End;
    if(!content){CoerceMethod(cl,self,OM_DISPOSE);return NULL;}
    DoMethod(self,OM_ADDMEMBER,(IPTR)content);populate(d,&initial);
    Object *objects[]={d->style,d->interval,d->timeout,d->history,d->wheel,d->fontname,d->pixels,d->semantic};
    for(unsigned i=0;i<sizeof(objects)/sizeof(objects[0]);i++)DoMethod(objects[i],MUIM_Notify,i?MUIA_String_Contents:MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,3,MUIM_Set,MUIA_PrefsEditor_Changed,TRUE);
    DoMethod(d->bindingSlot,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingSelect);
    DoMethod(d->bindingScreen,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingCursor,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingKeypad,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingSearch,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingMode,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingAction,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingSpecial,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->bindingKey,MUIM_Notify,MUIA_String_Contents,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    for(unsigned i=0;i<5;i++)DoMethod(d->bindingMods[i],MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleBindingEdit);
    DoMethod(d->cursorCustom,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleCursorColor);
    Object *cursorSliders[]={d->cursorRed,d->cursorGreen,d->cursorBlue};
    for(int i=0;i<3;i++)DoMethod(cursorSliders[i],MUIM_Notify,MUIA_Numeric_Value,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleCursorColor);
    DoMethod(d->emoji,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,3,MUIM_Set,MUIA_PrefsEditor_Changed,TRUE);
    DoMethod(d->bell,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,3,MUIM_Set,MUIA_PrefsEditor_Changed,TRUE);
    DoMethod(d->oscCopy,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,3,MUIM_Set,MUIA_PrefsEditor_Changed,TRUE);
    DoMethod(d->oscRead,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,3,MUIM_Set,MUIA_PrefsEditor_Changed,TRUE);
    DoMethod(d->load,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)self,1,MUIM_ConsoleLoad);
    DoMethod(d->write,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)self,1,MUIM_ConsoleWrite);
    DoMethod(d->presetApply,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)self,1,MUIM_ConsoleColorPreset);
    DoMethod(d->colorSlot,MUIM_Notify,MUIA_Cycle_Active,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleColorSelect);
    Object *sliders[]={d->red,d->green,d->blue};
    for(int i=0;i<3;i++)DoMethod(sliders[i],MUIM_Notify,MUIA_Numeric_Value,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleColorEdit);
    DoMethod(d->colorInherit,MUIM_Notify,MUIA_Selected,MUIV_EveryTime,(IPTR)self,1,MUIM_ConsoleColorInherit);
    DoMethod(d->choose,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)self,1,MUIM_ConsoleFont);
    DoMethod(d->inherit,MUIM_Notify,MUIA_Pressed,FALSE,(IPTR)self,1,MUIM_ConsoleInherit);
    return self;
}
IPTR ConsoleEditor__MUIM_ConsoleBindingSelect(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);d->bindingIndex=XGET(d->bindingSlot,MUIA_Cycle_Active);showBinding(d);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleBindingEdit(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);if(d->populating)return TRUE;
    struct ConsolengKeyBinding *b=&d->bindingRows[d->bindingIndex];
    const UBYTE *key=(void*)XGET(d->bindingKey,MUIA_String_Contents);
    ULONG special=XGET(d->bindingSpecial,MUIA_Cycle_Active);
    if(special>sizeof(specialCodes)/sizeof(*specialCodes))return FALSE;
    NNSET(d->bindingKey,MUIA_Disabled,special!=0 && special<sizeof(specialCodes)/sizeof(*specialCodes));
    b->kb_Key=special==sizeof(specialCodes)/sizeof(*specialCodes)?con_binding_hex_key((const char*)key):
        special?specialCodes[special]:(key && key[0] && !key[1]?key[0]:(ULONG)~(ULONG)0);
    if(b->kb_Key>='A'&&b->kb_Key<='Z')b->kb_Key+=32;
    b->kb_Action=XGET(d->bindingAction,MUIA_Cycle_Active);b->kb_Modifiers=0;
    for(unsigned i=0;i<5;i++)if(XGET(d->bindingMods[i],MUIA_Selected))b->kb_Modifiers|=1U<<i;
    ULONG mode=XGET(d->bindingMode,MUIA_Cycle_Active);
    b->kb_Modifiers|=mode==1?CONSOLENG_BIND_NAVONLY:mode==2?CONSOLENG_BIND_NOTNAV:0;
    mode=XGET(d->bindingScreen,MUIA_Cycle_Active);b->kb_Modifiers|=mode==1?CONSOLENG_BIND_ALTSCREEN:mode==2?CONSOLENG_BIND_PRIMARY:0;
    mode=XGET(d->bindingCursor,MUIA_Cycle_Active);b->kb_Modifiers|=mode==1?CONSOLENG_BIND_APPCURSOR:mode==2?CONSOLENG_BIND_NORMALCURSOR:0;
    mode=XGET(d->bindingKeypad,MUIA_Cycle_Active);b->kb_Modifiers|=mode==1?CONSOLENG_BIND_APPKEYPAD:mode==2?CONSOLENG_BIND_NORMALKEYPAD:0;
    mode=XGET(d->bindingSearch,MUIA_Cycle_Active);b->kb_Modifiers|=mode==1?CONSOLENG_BIND_SEARCHMODE:mode==2?CONSOLENG_BIND_NOTSEARCH:0;
    SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleCursorColor(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);showCursorColor(d);
    SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleColorPreset(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);
    if(!colorPresetApply(&d->colors,XGET(d->preset,MUIA_Cycle_Active)))return FALSE;
    showColor(d);SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleColorSelect(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);d->colorIndex=XGET(d->colorSlot,MUIA_Cycle_Active);showColor(d);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleColorEdit(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);if(d->populating)return TRUE;
    d->colors.cp_RGB[d->colorIndex]=(XGET(d->red,MUIA_Numeric_Value)<<16)|(XGET(d->green,MUIA_Numeric_Value)<<8)|XGET(d->blue,MUIA_Numeric_Value);
    NNSET(d->preset,MUIA_Cycle_Active,0);
    d->colors.cp_Mask|=1UL<<d->colorIndex;showColor(d);SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleColorInherit(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);if(d->populating)return TRUE;
    NNSET(d->preset,MUIA_Cycle_Active,0);
    if(XGET(d->colorInherit,MUIA_Selected))d->colors.cp_Mask&=~(1UL<<d->colorIndex);
    else {d->colors.cp_RGB[d->colorIndex]=baselineColors.cp_RGB[d->colorIndex];d->colors.cp_Mask|=1UL<<d->colorIndex;}
    showColor(d);SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleFont(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);
    ULONG pixels=XGET(d->pixels,MUIA_String_Integer);
    const char *name=(const char*)XGET(d->fontname,MUIA_String_Contents);
    struct FontRequester *req=AllocAslRequestTags(ASL_FontRequest,
        ASLFO_Window,(IPTR)_window(self),ASLFO_SleepWindow,TRUE,ASLFO_FixedWidthOnly,TRUE,
        ASLFO_InitialName,(IPTR)(name&&*name?name:"DejaVuSansMono.font"),
        ASLFO_InitialSize,pixels?pixels:16,TAG_DONE);
    if(!req){notice(self,"Could not open the font requester.");return FALSE;}
    if(AslRequestTags(req,TAG_DONE)){
        SET(d->fontname,MUIA_String_Contents,(IPTR)req->fo_Attr.ta_Name);
        SET(d->pixels,MUIA_String_Integer,req->fo_Attr.ta_YSize);
    }
    FreeAslRequest(req);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleInherit(Class *cl,Object *self,Msg msg)
{
    struct ConsoleEditor_DATA *d=INST_DATA(cl,self);
    SET(d->fontname,MUIA_String_Contents,(IPTR)"");SET(d->pixels,MUIA_String_Integer,0);return TRUE;
}
IPTR ConsoleEditor__MUIM_ConsoleLoad(Class *cl,Object *self,Msg msg)
{
    return DoMethod(_win(self),MUIM_PrefsWindow_Import);
}
IPTR ConsoleEditor__MUIM_ConsoleWrite(Class *cl,Object *self,Msg msg)
{
    return DoMethod(_win(self),MUIM_PrefsWindow_Export,FALSE);
}
IPTR ConsoleEditor__MUIM_Setup(Class *cl,Object *self,struct MUIP_Setup *msg)
{
    if(!DoSuperMethodA(cl,self,(Msg)msg))return FALSE;
    populate(INST_DATA(cl,self),&initial);setupComplete=1;
    SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_Import(Class *cl,Object *self,struct MUIP_PrefsEditor_Import *msg)
{
    struct ConsoleSettings p=initial;
    if(!settingsLoad((const char*)msg->filename,&p)) {if(setupComplete)notice(self,"Invalid or unreadable ConsoleNG profile. Settings were not changed.");return FALSE;}
    populate(INST_DATA(cl,self),&p);return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_Export(Class *cl,Object *self,struct MUIP_PrefsEditor_Export *msg)
{
    struct ConsoleSettings p;
    if(!collect(INST_DATA(cl,self),&p)){notice(self,"Invalid settings. Check numeric values, shortcuts and word separators.\nSeparators must be unique characters or valid Unicode escapes.");return FALSE;}
    BPTR lock=Lock(msg->filename,ACCESS_READ);
    if(lock){UnLock(lock);if(MUI_Request(_app(self),_win(self),0,(CONST_STRPTR)"Replace profile?",(CONST_STRPTR)"Replace|Cancel",(CONST_STRPTR)"Replace the selected profile file?")!=1)return FALSE;}
    if(!settingsSave((const char*)msg->filename,&p)){notice(self,"Could not write the profile. Existing file was preserved.");return FALSE;}
    return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_ImportFH(Class *cl,Object *self,struct MUIP_PrefsEditor_ImportFH *msg)
{
    char buf[4096];LONG n=Read(msg->fh,buf,sizeof(buf)-1);struct ConsoleSettings p=initial;
    if(n<0||n==(LONG)sizeof(buf)-1)return FALSE;
    buf[n]=0;
    if(strlen(buf)!=(size_t)n||!settingsParse(buf,&p))return FALSE;
    populate(INST_DATA(cl,self),&p);return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_ExportFH(Class *cl,Object *self,struct MUIP_PrefsEditor_ExportFH *msg)
{
    char buf[4096];struct ConsoleSettings p;if(!collect(INST_DATA(cl,self),&p))return FALSE;
    int n=settingsFormat(buf,sizeof(buf),&p);return n&&Write(msg->fh,buf,n)==n;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_SetDefaults(Class *cl,Object *self,Msg msg)
{
    struct ConsoleSettings p={{sizeof(p.cursor),1,0,500,0},{sizeof(p.history),1,1000},{sizeof(p.font),1,0,{0}},{sizeof(p.colors),1,0,{0}},1,1};
    p.cursorRGB=0x12a4e8;p.wheelLines=1;p.semantic=con_semantic_defaults();
    con_binding_prefs_defaults(&p.bindings);
    populate(INST_DATA(cl,self),&p);return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_Test(Class *cl,Object *self,Msg msg)
{
    struct ConsoleSettings p;if(!collect(INST_DATA(cl,self),&p)||!apply(preview,0,&p)){notice(self,"Could not preview settings. Check numeric fields and shortcut keys.\nEach shortcut needs a unique key and modifier combination.\nWord separators must be unique characters or valid Unicode escapes.");return FALSE;}
    preview->io_Command=CMD_WRITE;preview->io_Data=(APTR)"\7";preview->io_Length=1;DoIO((struct IORequest*)preview);
    SET(self,MUIA_PrefsEditor_Testing,TRUE);puts("ZUNE_PREFS_PREVIEW");fflush(stdout);return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_Revert(Class *cl,Object *self,Msg msg)
{
    if(!apply(preview,0,&initial))return FALSE;
    populate(INST_DATA(cl,self),&initial);SET(self,MUIA_PrefsEditor_Testing,FALSE);SET(self,MUIA_PrefsEditor_Changed,TRUE);return TRUE;
}
static IPTR useSettings(Class *cl,Object *self,int persist)
{
    struct ConsoleSettings p;if(!collect(INST_DATA(cl,self),&p)){notice(self,"Invalid settings. Check numeric values, shortcuts and word separators.\nSeparators must be unique characters or valid Unicode escapes.");return FALSE;}
    if(!validFont(&p)){notice(self,"Font unavailable: choose an installed fixed-width font and an exact supported pixel height.");return FALSE;}
    if(persist&&!settingsSave(ARCFILE,&p)){notice(self,"Could not write ENVARC:. Device defaults were not changed.");return FALSE;}
    if(!settingsSave(ENVFILE,&p)){notice(self,persist?"ENVARC: saved, but ENV: failed. Device defaults were not changed.":"Could not write ENV:. Device defaults were not changed.");return FALSE;}
    if(!apply(control,1,&p)){notice(self,"Settings were saved, but the device rejected them.");return FALSE;}
    return TRUE;
}
IPTR ConsoleEditor__MUIM_PrefsEditor_Use(Class *cl,Object *self,Msg msg){return useSettings(cl,self,0);}
IPTR ConsoleEditor__MUIM_PrefsEditor_Save(Class *cl,Object *self,Msg msg){return useSettings(cl,self,1);}
IPTR ConsoleEditor__MUIM_PrefsEditor_Cancel(Class *cl,Object *self,Msg msg){return TRUE;}
ZUNE_CUSTOMCLASS_23(ConsoleEditor,NULL,(CONST_STRPTR)MUIC_PrefsEditor,NULL,
    MUIM_ConsoleBindingSelect,Msg,MUIM_ConsoleBindingEdit,Msg,
    MUIM_ConsoleCursorColor,Msg,MUIM_ConsoleColorPreset,Msg,MUIM_ConsoleColorSelect,Msg,MUIM_ConsoleColorEdit,Msg,MUIM_ConsoleColorInherit,Msg,
    MUIM_ConsoleLoad,Msg,MUIM_ConsoleWrite,Msg,MUIM_ConsoleFont,Msg,MUIM_ConsoleInherit,Msg,
    OM_NEW,struct opSet *,MUIM_Setup,struct MUIP_Setup *,
    MUIM_PrefsEditor_Import,struct MUIP_PrefsEditor_Import *,MUIM_PrefsEditor_Export,struct MUIP_PrefsEditor_Export *,
    MUIM_PrefsEditor_ImportFH,struct MUIP_PrefsEditor_ImportFH *,MUIM_PrefsEditor_ExportFH,struct MUIP_PrefsEditor_ExportFH *,
    MUIM_PrefsEditor_SetDefaults,Msg,MUIM_PrefsEditor_Test,Msg,MUIM_PrefsEditor_Revert,Msg,
    MUIM_PrefsEditor_Use,Msg,MUIM_PrefsEditor_Save,Msg,MUIM_PrefsEditor_Cancel,Msg);
int main(int argc,char **argv)
{
    int rc=20,opened=0,previewOpened=0;struct MsgPort *port=NULL;struct Window *sample=NULL;Object *app=NULL;
    if(!parseOptions(argc,argv,&options)){fprintf(stderr,"ConsoleNG [FROM <profile>] [USE | SAVE | RESTORE]\n");return 20;}
    port=CreateMsgPort();if(!port)goto out;
    control=(void*)CreateIORequest(port,sizeof(*control));preview=(void*)CreateIORequest(port,sizeof(*preview));if(!control||!preview)goto out;
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0))goto out;
    opened=1;
    struct ConsolengSelectionProfile selection;
    if(!exchange(control,CD_GETSELECTIONPROFILE,&selection,sizeof(selection)))goto out;
    struct ConsolengInputProfile input=selection.sp_Input;
    initial.semantic=selection.sp_Semantic;
    struct ConsolengInteractionProfile full=input.ip_Interaction;
    initial.wheelLines=input.ip_Scroll.sp_Lines;
    struct ConsolengAppearanceProfile snapshot=full.ip_Appearance;
    initial.bindings=full.ip_Bindings;
    initial.cursorCustom=snapshot.rp_CursorColor.ccp_Enabled;initial.cursorRGB=snapshot.rp_CursorColor.ccp_RGB;
    initial.cursor=snapshot.rp_Theme.tp_Cursor;initial.history=snapshot.rp_Theme.tp_History;
    initial.font=snapshot.rp_Theme.tp_Font;initial.colors=snapshot.rp_Theme.tp_Colors;
    initial.emoji=snapshot.rp_Emoji.ep_Enabled;
    initial.bell=snapshot.rp_Bell.bp_Enabled;
    initial.oscCopy=snapshot.rp_Clipboard.cbp_Enabled;
    int loaded=options.from?settingsLoad(options.from,&initial):
        settingsLoad(ENVFILE,&initial)||settingsLoad(ARCFILE,&initial)||
        settingsLoad("ENV:SYS/consoleng-cursor.prefs",&initial)||settingsLoad("ENVARC:SYS/consoleng-cursor.prefs",&initial);
    if(!loaded&&(options.from||options.mode!=MODE_GUI)){fprintf(stderr,"ConsoleNG: no valid profile\n");goto out;}
    if(options.mode!=MODE_GUI){
        if(!validFont(&initial)){fprintf(stderr,"ConsoleNG: unavailable font\n");goto out;}
        if(options.mode==MODE_SAVE&&!settingsSave(ARCFILE,&initial))goto out;
        if((options.mode==MODE_SAVE||options.mode==MODE_USE)&&!settingsSave(ENVFILE,&initial))goto out;
        if(apply(control,1,&initial))rc=0;
        goto out;
    }
    sample=OpenWindowTags(NULL,WA_Left,40,WA_Top,340,WA_Width,710,WA_Height,200,WA_Title,(IPTR)"ConsoleNG live preview",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_MinWidth,200,WA_MinHeight,120,WA_MaxWidth,~0UL,WA_MaxHeight,~0UL,TAG_DONE);if(!sample)goto out;
    preview->io_Data=sample;preview->io_Length=sizeof(*sample);
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)preview,0))goto out;
    previewOpened=1;
    struct ConsolengColorPrefs inherited={sizeof(inherited),1,0,{0}};
    if(!exchange(preview,CD_SETCOLORPREFS,&inherited,sizeof(inherited))||!exchange(preview,CD_GETCOLORPREFS,&baselineColors,sizeof(baselineColors)))goto out;
    const char *text="\x1b%G\r\n  ConsoleNG native Preferences\r\n\r\n  Test changes this console only.\r\n  Cancel leaves device defaults unchanged.\r\n\r\n  ANSI: \x1b[38;5;1mRed  \x1b[38;5;2mGreen  \x1b[38;5;4mBlue  \x1b[38;5;11mBright yellow\x1b[39m\r\n  Amiga pens: \x1b[31m1  \x1b[32m2  \x1b[34m4\x1b[39m (Workbench palette)\r\n  Emoji: \xf0\x9f\x98\x80 \xf0\x9f\x9a\x80 \xf0\x9f\x90\xb1\r\n  Cursor preview: ";
    preview->io_Command=CMD_WRITE;preview->io_Data=(APTR)text;preview->io_Length=strlen(text);if(DoIO((struct IORequest*)preview)||!apply(preview,0,&initial))goto out;
    Object *window,*editor;
    app=(Object*)ApplicationObject,MUIA_Application_Title,(IPTR)"ConsoleNG Preferences",MUIA_Application_Base,(IPTR)"CONSOLENGPREFS",
        SubWindow,(IPTR)(window=(Object*)SystemPrefsWindowObject,MUIA_Window_ID,MAKE_ID('C','N','G','P'),MUIA_Window_LeftEdge,50,MUIA_Window_TopEdge,20,
            WindowContents,(IPTR)(editor=NewObject(ConsoleEditor_CLASS->mcc_Class,NULL,TAG_DONE)),End),End;
    if(!app)goto out;
    SET(window,MUIA_Window_Open,TRUE);if(!XGET(window,MUIA_Window_Open))goto out;
    uiReady=1;DoMethod(editor,MUIM_ConsoleCursorColor);
    puts("ZUNE_PREFS_READY");fflush(stdout);DoMethod(app,MUIM_Application_Execute);rc=0;
out:
    if(app)MUI_DisposeObject(app);
    if(previewOpened)CloseDevice((struct IORequest*)preview);
    if(sample)CloseWindow(sample);
    if(opened)CloseDevice((struct IORequest*)control);
    if(preview)DeleteIORequest((struct IORequest*)preview);
    if(control)DeleteIORequest((struct IORequest*)control);
    if(port)DeleteMsgPort(port);
    if(options.mode==MODE_GUI)printf("ZUNE_PREFS_DONE rc=%d\n",rc);
    else if(rc)fprintf(stderr,"ConsoleNG: settings operation failed\n");
    return rc;
}
