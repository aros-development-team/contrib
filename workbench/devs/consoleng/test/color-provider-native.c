/* Private FreeType color-map provider acceptance, not ConsoleNG fallback yet. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/bullet.h>
#include <proto/cybergraphics.h>
#include <diskfont/glyph.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>
#include <stdio.h>
struct Library *BulletBase,*CyberGfxBase;
#define OT_Spec1_FontFile (OT_Spec1|OT_Indirect)
#define CHECK(c) do{if(!(c)){printf("COLORPROVIDER_FAIL line=%d size=%d index=%d\n",__LINE__,size,index);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,size=0,index=0,cases=0;struct Window*w=NULL;struct GlyphEngine*ge=NULL;struct ColorGlyphMap*map=NULL;
 BulletBase=OpenLibrary((CONST_STRPTR)"EMOJI:Libs/freetype2.library",0);CHECK(BulletBase);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"AROS native Noto Color Emoji provider",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,TAG_DONE);CHECK(w);
 ge=OpenEngine();CHECK(ge);struct TagItem font[]={{OT_Spec1_FontFile,(IPTR)"EMOJI:NotoColorEmoji.ttf"},{OT_DeviceDPI,(72<<16)|72},{TAG_DONE,0}};CHECK(SetInfoA(ge,font)==OTERR_Success);
 ULONG cps[]={0x1f600,0x1f680,0x1f431,0x1f30d,0x1f389,0x2764};int sizes[]={16,24,32,48};
 for(size=0;size<4;size++)for(index=0;index<6;index++){
 struct TagItem set[]={{OT_PointHeight,sizes[size]<<16},{OT_GlyphCode,cps[index]},{TAG_DONE,0}};CHECK(SetInfoA(ge,set)==OTERR_Success);
 struct TagItem get[]={{OT_ColorGlyphMap32,(IPTR)&map},{TAG_DONE,0}};ULONG result=ObtainInfoA(ge,get);if(result)printf("COLORPROVIDER_OBTAIN rc=%lu\n",(unsigned long)result);CHECK(result==OTERR_Success&&map&&map->cgm_ARGBMap);CHECK(map->cgm_BMRows<=sizes[size]&&map->cgm_BMRows&&map->cgm_BlackWidth&&map->cgm_BMModulo==map->cgm_BlackWidth*4);
 UBYTE*b=(UBYTE*)map->cgm_ARGBMap;int ink=0,color=0;for(ULONG k=0;k<(ULONG)map->cgm_BMRows*map->cgm_BMModulo;k+=4)if(b[k]){ink++;if(b[k+1]!=b[k+2]||b[k+2]!=b[k+3])color++;}CHECK(ink&&color);
 WritePixelArrayAlpha(map->cgm_ARGBMap,0,0,map->cgm_BMModulo,w->RPort,50+index*100,65+size*85,map->cgm_BlackWidth,map->cgm_BMRows,0xffffffff);
 struct TagItem release[]={{OT_ColorGlyphMap32,(IPTR)map},{TAG_DONE,0}};CHECK(ReleaseInfoA(ge,release)==OTERR_Success);map=NULL;cases++;
 }
 CloseEngine(ge);ge=OpenEngine();CHECK(ge);font[0].ti_Data=(IPTR)"EMOJI:DejaVuSansMono.ttf";CHECK(SetInfoA(ge,font)==OTERR_Success);
 for(size=0;size<4;size++)for(index=0;index<6;index++){
 struct GlyphMap*mono=NULL;struct TagItem set[]={{OT_PointHeight,sizes[size]<<16},{OT_GlyphCode,index%2?0x3c0:'A'},{TAG_DONE,0}},get[]={{OT_GlyphMap,(IPTR)&mono},{TAG_DONE,0}};CHECK(SetInfoA(ge,set)==OTERR_Success);CHECK(ObtainInfoA(ge,get)==OTERR_Success&&mono&&mono->glm_BitMap&&mono->glm_BlackWidth);struct TagItem release[]={{OT_GlyphMap,(IPTR)mono},{TAG_DONE,0}};CHECK(ReleaseInfoA(ge,release)==OTERR_Success);
 }
 Move(w->RPort,20,440);Text(w->RPort,(CONST_STRPTR)"PASS: 24 color maps + 24 existing monochrome maps",47);
 printf("COLORPROVIDER_PASS color=%d mono=24\n",cases);fflush(stdout);Delay(600);rc=0;
 out:if(map&&ge){struct TagItem release[]={{OT_ColorGlyphMap32,(IPTR)map},{TAG_DONE,0}};ReleaseInfoA(ge,release);}if(ge)CloseEngine(ge);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(BulletBase)CloseLibrary(BulletBase);printf("COLORPROVIDER_SCRIPT rc=%d\n",rc);return rc;
}
