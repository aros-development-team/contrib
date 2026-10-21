/* Standalone native monochrome glyph probe; compile with -I.. to exercise
 * the actual ConsoleNG engine binding without installing a device. */
#include <proto/diskfont.h>
#include <stdio.h>
#include "consolengglyph.c"
int main(void) {
 struct ConsoleBase base = {0}; struct ConsoleBase *ConsoleDevice=&base;
 struct TextAttr ta={(STRPTR)"DejaVuSansMono.font",14,0,0};
 struct TextFont *font=OpenDiskFont(&ta);
 ULONG cps[]={0x03c0,0x2615,0x26a1};
 base.cb_UtilityBase=OpenLibrary("utility.library",0);
 InitSemaphore(&base.glyphLock);
 if(!font||!base.cb_UtilityBase)return 20;
 printf("FONT y=%u x=%u baseline=%u name=%s\n",font->tf_YSize,font->tf_XSize,font->tf_Baseline,font->tf_Message.mn_Node.ln_Name);
 LONG why=ConsolengGlyphBind(&base,font);printf("BIND %ld\n",why);
 if(why==0) {
  struct Library *BulletBase=base.glyphEngineBase;
  for(int i=0;i<3;i++) {
   struct GlyphMap *gm=NULL;
   struct TagItem st[]={{OT_GlyphCode,cps[i]},{TAG_DONE,0}},gt[]={{OT_GlyphMap,(IPTR)&gm},{TAG_DONE,0}};
   ULONG sr=SetInfoA(base.glyphEngine,st),gr=ObtainInfoA(base.glyphEngine,gt);
   printf("CP %lx set=%lu get=%lu map=%p\n",cps[i],sr,gr,gm);
   if(gm) {
    printf("MAP w=%u rows=%u pitch=%u left=%u top=%u x0=%d y0=%d\n",gm->glm_BlackWidth,gm->glm_BMRows,gm->glm_BMModulo,gm->glm_BlackLeft,gm->glm_BlackTop,gm->glm_X0,gm->glm_Y0);
    for(int y=0;y<gm->glm_BMRows;y++) { for(int x=0;x<gm->glm_BlackWidth;x++) {unsigned char v=((unsigned char*)gm->glm_BitMap)[y*gm->glm_BMModulo+((gm->glm_BlackLeft+x)>>3)];putchar((v & (0x80 >> ((gm->glm_BlackLeft+x)&7)))?'#':'.');}putchar('\n'); }
    struct TagItem rt[]={{OT_GlyphMap,(IPTR)gm},{TAG_DONE,0}};ReleaseInfoA(base.glyphEngine,rt);
   }
  }
 }
 ConsolengGlyphUnbind(&base);CloseFont(font);CloseLibrary(base.cb_UtilityBase);return why?20:0;
}
