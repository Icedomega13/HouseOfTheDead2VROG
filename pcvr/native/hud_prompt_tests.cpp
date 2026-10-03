#include "hud_prompt.h"
#include <cstdio>
#include <cstdlib>
using namespace hotd2_hud;
static unsigned checks=0;
static void require(bool ok,const char* name){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",name);exit(1);}}
static void quad(Vertex* p,float left,float top,float right,float bottom) {
    p[0]={left,bottom,.19994f,1.0001f,0xffffffff,0,0,0};
    p[1]={right,bottom,.19994f,1.0001f,0xffffffff,0,1,0};
    p[2]={left,top,.19994f,1.0001f,0xffffffff,0,0,1};
    p[3]={right,top,.19994f,1.0001f,0xffffffff,0,1,1};
}
int main() {
    PromptFilter filter;filter.begin(1);Vertex p[4];
    quad(p,384,412,563.2f,434.4f);
    require(filter.hide(p,4,128,16,Text::Join),"captured player-two join bitmap hidden");
    require(!filter.hide(p,4,128,16,Text::Other),"unrecognized message in footer preserved");
    require(!filter.hide(p,4,128,32,Text::Join),"wrong texture shape preserved");
    require(!filter.hide(p,3,128,16,Text::Join),"non-quad geometry preserved");
    quad(p,64,412,243.2f,434.4f);
    require(!filter.hide(p,4,128,16,Text::Join),"player-one join message preserved");
    quad(p,224,320,403.2f,342.4f);
    require(!filter.hide(p,4,128,16,Text::Join),"title start prompt preserved");
    quad(p,518,427,531.6f,454.2f);
    require(!filter.hide(p,4,16,32,Text::Other),"unassociated digit preserved");
    quad(p,424,427,532.8f,454.2f);
    require(filter.hide(p,4,128,32,Text::Credits),"captured player-two credits label hidden");
    quad(p,518,427,531.6f,454.2f);
    require(filter.hide(p,4,16,32,Text::Other),"associated credit digit hidden");
    quad(p,531.6f,427,545.2f,454.2f);
    require(filter.hide(p,4,16,32,Text::Other),"second credit digit supported");
    quad(p,450,427,463.6f,454.2f);
    require(!filter.hide(p,4,16,32,Text::Other),"other footer text preserved");
    quad(p,518,400,531.6f,427.2f);
    require(!filter.hide(p,4,16,32,Text::Other),"different text row preserved");
    quad(p,518,427,531.6f,454.2f);filter.begin(2);
    require(filter.hide(p,4,16,32,Text::Other),"count preceding label uses previous presentation row");
    filter.begin(3);
    require(!filter.hide(p,4,16,32,Text::Other),"credit association expires without continuing label");
    filter.begin(9);
    require(!filter.hide(p,4,16,32,Text::Other),"presentation gap cannot retain credit row");
    quad(p,32,427,140.8f,454.2f);
    require(!filter.hide(p,4,128,32,Text::Credits),"player-one credit label preserved");
    quad(p,124,444,156,476);
    require(!filter.hide(p,4,32,32,Text::Other),"ammo icons retain placement and rendering");
    quad(p,384,412,563.2f,434.4f);p[0].x=NAN;
    require(!filter.hide(p,4,128,16,Text::Join),"invalid coordinates preserve draw");
    quad(p,384,412,563.2f,434.4f);p[0].u=.5f;
    require(!filter.hide(p,4,128,16,Text::Join),"partial text atlas preserved");
    quad(p,384,412,563.2f,434.4f);p[3]=p[2];
    require(!filter.hide(p,4,128,16,Text::Join),"duplicate corners preserved");
    quad(p,384,412,563.2f,434.4f);p[0].z=.9f;
    require(!filter.hide(p,4,128,16,Text::Join),"world depth quad preserved");
    quad(p,384,412,563.2f,434.4f);p[0].rhw=.5f;
    require(!filter.hide(p,4,128,16,Text::Join),"different projection preserved");
    require(text_identity("3666b222f8a0a64b862c4597e5ba4f8300ba3e283bcc6616eebdbee9d30589a2")==Text::Join,"exact join identity");
    require(text_identity("e2ee80a79481fe8dea2918bb29044f975a434f8b2edd93d11c550eede02c43e8")==Text::Credits,"exact credits identity");
    require(text_identity("9868a95bb15eaf8b009890e3ad6dc416fb273274cb5ad478b38242ce2b5f39d2")==Text::Other,"continue graphic preserved");
    require(text_identity("b57495c040ac67156056b8be26bddec01796425eb7cd52548b6e867ea005594f")==Text::Other,"game-over graphic preserved");
    require(text_identity(nullptr)==Text::Other,"unknown identity preserved");
    auto digit=text_identity("2c3169048b14d83c03789820105940798e1096be9f7d709476ad643eaa5e6f41");
    require(digit==Text::CreditDigit,"captured five content identity");
    PromptFilter fresh;fresh.begin(1);quad(p,518,427,531.6f,454.2f);
    require(fresh.hide(p,4,16,32,digit),"five preceding first label is hidden");
    fresh.begin(90);
    require(fresh.hide(p,4,16,32,digit),"five remains hidden throughout label blink gap");
    quad(p,531.6f,427,545.2f,454.2f);
    require(fresh.hide(p,4,16,32,digit),"authenticated second counter slot hidden");
    quad(p,450,427,463.6f,454.2f);
    require(!fresh.hide(p,4,16,32,digit),"same glyph outside counter slots preserved");
    quad(p,518,400,531.6f,427.2f);
    require(!fresh.hide(p,4,16,32,digit),"same glyph on different row preserved");
    quad(p,518,427,531.6f,454.2f);
    require(!fresh.hide(p,4,32,32,digit),"same-sized ammo quad cannot become credit digit");
    auto cursor=text_identity("573332fb46858a9e9eb6976cc0c88d2a022c58525692f6bd9a76d2d34f9e7405");
    require(cursor==Text::AimCursor,"observed red crosshair identity");
    quad(p,391,174,423,206);for(auto& vertex:p){vertex.z=.20002f;vertex.rhw=1;}
    require(aim_cursor_quad(p,4),"captured native cursor geometry recognized");
    for(auto& vertex:p)vertex.x+=90;
    require(aim_cursor_quad(p,4),"cursor geometry does not depend on live or stale aiming coordinates");
    require(!aim_cursor_quad(p,3),"non-quad remains native");
    p[0].x=NAN;require(!aim_cursor_quad(p,4),"invalid cursor coordinates remain native");
    quad(p,391,174,423,206);for(auto& vertex:p){vertex.z=.20002f;vertex.rhw=1;}
    p[0].u=.5f;require(!aim_cursor_quad(p,4),"partial sprite UVs remain native");
    quad(p,391,174,423,206);for(auto& vertex:p){vertex.z=.20002f;vertex.rhw=1;}p[3]=p[2];
    require(!aim_cursor_quad(p,4),"duplicate cursor corners rejected");
    quad(p,391,174,423,206);for(auto& vertex:p){vertex.z=.20002f;vertex.rhw=1;}p[0].z=.3f;
    require(!aim_cursor_quad(p,4),"other depth sprite remains native");
    quad(p,28,412,60,444);
    require(native_status_quad(p,4,Text::NativeHealth),"captured native 1P health slot recognized");
    require(!native_status_quad(p,4,Text::Other),"same footer with unrelated texture stays visible");
    quad(p,60,412,92,444);require(native_status_quad(p,4,Text::NativeHealth),"animated health icon slot recognized");
    quad(p,24,364,72,460);require(native_status_quad(p,4,Text::NativeAmmo),"native cartridge row recognized");
    quad(p,204,364,252,460);require(native_status_quad(p,4,Text::NativeAmmo),"sixth cartridge slot recognized");
    quad(p,24,364,56,428);require(native_status_quad(p,4,Text::NativeAmmo),"captured active-game cartridge geometry recognized");
    quad(p,144,364,176,428);require(native_status_quad(p,4,Text::NativeAmmo),"sixth active-game cartridge slot recognized");
    quad(p,24,364,64,444);require(native_status_quad(p,4,Text::NativeAmmo),"intermediate native cartridge scale retains exact aspect and row");
    quad(p,24,364,56,430);require(!native_status_quad(p,4,Text::NativeAmmo),"distorted cartridge aspect remains native");
    quad(p,24,364,88,492);require(!native_status_quad(p,4,Text::NativeAmmo),"oversized cartridge draw remains native");
    quad(p,316,412,348,444);require(native_status_quad(p,4,Text::NativeHealth),"ninth health slot covered without crossing player-two region");
    quad(p,192,268,448,300);require(!native_status_quad(p,4,Text::NativeAmmo),"central RELOAD warning remains native");
    quad(p,16,325,272,389);require(!native_status_quad(p,4,Text::NativeAmmo),"secondary reload instruction remains native");
    quad(p,384,412,416,444);require(!native_status_quad(p,4,Text::NativeHealth),"player-two HUD stays native");
    quad(p,60,240,92,272);require(!native_status_quad(p,4,Text::NativeHealth),"different dialogue row remains native");
    quad(p,60,412,92,444);p[0].z=.7f;require(!native_status_quad(p,4,Text::NativeHealth),"world-depth sprite remains native");
    quad(p,60,412,92,444);p[0].x=NAN;require(!native_status_quad(p,4,Text::NativeHealth),"invalid status vertex fails open");
    require(!native_status_quad(p,3,Text::NativeHealth),"non-quad remains visible");
    quad(p,60,412,92,444);p[0].u=.5f;require(!native_status_quad(p,4,Text::NativeHealth),"partial UV sprite stays native");
    quad(p,61,412,93,444);require(!native_status_quad(p,4,Text::NativeHealth),"off-grid slot stays native");
    quad(p,60,412,92,444);p[3]=p[2];require(!native_status_quad(p,4,Text::NativeHealth),"duplicate corners stay native");
    require(text_identity("b84bd7d30ed78278fd380229eb323cf430460f10c52a459819e4005b779c6ac1")==Text::NativeHealth,"observed 1P bitmap authenticated");
    for(auto key:{"027d5afa39b6b1814f70bc81e6836edd84f46a237fb1e026148408d030e863af",
        "2c1b6ec090ea2a2b792cd874b8d01d7e87b5a18704e10dc7cc65411cca967714",
        "40ff53834af83f9fc7c8d252a87c679eed39d6d498e1ccf42319b6cb2dd912cc",
        "980a803433292ad6d0e6cb29819699077daf52d72a7279cb5c876a04cf2670cd",
        "a0a008607371802b99a45bde7b8e74f95baff8e115a0960cb25bad8c540d41a6",
        "b7d11e9ff0370f7f7dff4bf891704a30a7ee2ebc360d77339fb5f7ffa8792329",
        "da6affad8fd6419188d25a25c65c671d40754eda2987ccdbf1e15b8e6aefe631"})require(text_identity(key)==Text::NativeHealth,"all native candle animation identities covered");
    require(text_identity("e56c993196b897b23fb4355e0a7f699510c342161d9fa414efb76d132df987c6")==Text::NativeAmmo,"native cartridge authenticated");
    require(text_identity("af7e971e7ef064ea5ea6ffc41368b97d862197d23aa0dc8531299e3638f300e4")==Text::Other,"reload instruction never classified as ammo");
    printf("PASS %u native HUD prompt checks\n",checks);return 0;
}
