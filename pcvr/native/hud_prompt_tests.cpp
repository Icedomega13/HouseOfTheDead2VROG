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
    printf("PASS %u native HUD prompt checks\n",checks);return 0;
}
