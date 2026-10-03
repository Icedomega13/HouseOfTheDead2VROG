#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace hotd2_hud {
struct Vertex {float x,y,z,rhw;uint32_t diffuse,specular;float u,v;};
static_assert(sizeof(Vertex)==32,"Observed native RHW stride");
struct Bounds {float left=0,right=0,top=0,bottom=0,z=0,rhw=0;};
enum class Text {Other,Join,Credits,CreditDigit,AimCursor,NativeHealth,NativeAmmo};
inline bool native_status_quad(const void* data,unsigned count,Text text){
    if((text!=Text::NativeHealth&&text!=Text::NativeAmmo)||!data||count!=4)return false;
    auto p=static_cast<const Vertex*>(data);
    float left=p[0].x,top=p[0].y,right=left,bottom=top;unsigned corners=0;
    for(unsigned i=0;i<4;++i){
        if(!std::isfinite(p[i].x)||!std::isfinite(p[i].y)||!std::isfinite(p[i].z)||!std::isfinite(p[i].rhw)||
           !std::isfinite(p[i].u)||!std::isfinite(p[i].v)||
           std::fabs(p[i].z-.20002f)>.0002f||std::fabs(p[i].rhw-1)>.0002f)return false;
        bool r=std::fabs(p[i].u-1)<.0001f,t=std::fabs(p[i].v-1)<.0001f;
        if((!r&&std::fabs(p[i].u)>.0001f)||(!t&&std::fabs(p[i].v)>.0001f))return false;
        corners|=1u<<((r?1u:0u)+(t?2u:0u));
        left=std::min(left,p[i].x);right=std::max(right,p[i].x);top=std::min(top,p[i].y);bottom=std::max(bottom,p[i].y);
    }
    if(corners!=15)return false;
    // Native cartridges begin at 1.5 scale, then use 1.0 during gameplay.
    // Preserve the authenticated row/aspect/slot spacing across that range.
    float scale=text==Text::NativeHealth?1.0f:(right-left)/32.0f;
    if(scale<1.0f-.001f||scale>1.5f+.001f)return false;
    float width=32.0f*scale,height=(text==Text::NativeHealth?32.0f:64.0f)*scale;
    float expected_top=text==Text::NativeHealth?412.0f:364.0f;
    float start=text==Text::NativeHealth?28.0f:24.0f,step=text==Text::NativeHealth?32.0f:24.0f*scale;
    if(std::fabs(top-expected_top)>.1f||std::fabs(right-left-width)>.1f||std::fabs(bottom-top-height)>.1f||
       left<start-.1f||left>(text==Text::NativeHealth?316.0f:24.0f+5*step)+.1f||std::fabs((left-start)/step-std::round((left-start)/step))>.005f)return false;
    for(unsigned i=0;i<4;++i)if(std::fabs(p[i].x-(p[i].u>.5f?right:left))>.1f||std::fabs(p[i].y-(p[i].v>.5f?top:bottom))>.1f)return false;
    return true;
}
inline Text text_identity(const char* key) {
    if(!key) return Text::Other;
    if(!std::strcmp(key,"3666b222f8a0a64b862c4597e5ba4f8300ba3e283bcc6616eebdbee9d30589a2")) return Text::Join;
    if(!std::strcmp(key,"e2ee80a79481fe8dea2918bb29044f975a434f8b2edd93d11c550eede02c43e8")) return Text::Credits;
    if(!std::strcmp(key,"2c3169048b14d83c03789820105940798e1096be9f7d709476ad643eaa5e6f41")) return Text::CreditDigit;
    if(!std::strcmp(key,"573332fb46858a9e9eb6976cc0c88d2a022c58525692f6bd9a76d2d34f9e7405")) return Text::AimCursor;
    // Original arcade 1P label and all seven candle animation frames.
    for(auto health:{"b84bd7d30ed78278fd380229eb323cf430460f10c52a459819e4005b779c6ac1",
        "027d5afa39b6b1814f70bc81e6836edd84f46a237fb1e026148408d030e863af",
        "2c1b6ec090ea2a2b792cd874b8d01d7e87b5a18704e10dc7cc65411cca967714",
        "40ff53834af83f9fc7c8d252a87c679eed39d6d498e1ccf42319b6cb2dd912cc",
        "980a803433292ad6d0e6cb29819699077daf52d72a7279cb5c876a04cf2670cd",
        "a0a008607371802b99a45bde7b8e74f95baff8e115a0960cb25bad8c540d41a6",
        "b7d11e9ff0370f7f7dff4bf891704a30a7ee2ebc360d77339fb5f7ffa8792329",
        "da6affad8fd6419188d25a25c65c671d40754eda2987ccdbf1e15b8e6aefe631"}) if(!std::strcmp(key,health))return Text::NativeHealth;
    if(!std::strcmp(key,"e56c993196b897b23fb4355e0a7f699510c342161d9fa414efb76d132df987c6"))return Text::NativeAmmo;
    return Text::Other;
}
inline bool footer_quad(const void* data,unsigned count,Bounds& out) {
    if(!data||count!=4) return false;
    auto vertices=static_cast<const Vertex*>(data);auto first=vertices[0];
    out={first.x,first.x,first.y,first.y,first.z,first.rhw};unsigned corners=0;
    for(unsigned i=0;i<4;++i) {
        const auto& p=vertices[i];
        if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||!std::isfinite(p.rhw)||
            !std::isfinite(p.u)||!std::isfinite(p.v)||p.x<350||p.x>600||p.y<400||p.y>480||
            std::fabs(p.z-.2f)>.025f||std::fabs(p.rhw-1)>.025f||
            std::fabs(p.z-first.z)>.0001f||std::fabs(p.rhw-first.rhw)>.0001f) return false;
        bool right=std::fabs(p.u-1)<.0001f,top=std::fabs(p.v-1)<.0001f;
        if((!right&&std::fabs(p.u)>.0001f)||(!top&&std::fabs(p.v)>.0001f)) return false;
        corners|=1u<<((right?1u:0u)+(top?2u:0u));
        out.left=std::min(out.left,p.x);out.right=std::max(out.right,p.x);
        out.top=std::min(out.top,p.y);out.bottom=std::max(out.bottom,p.y);
    }
    if(corners!=15||out.right<=out.left||out.bottom<=out.top) return false;
    for(unsigned i=0;i<4;++i) {
        const auto& p=vertices[i];
        if(std::fabs(p.x-(p.u>.5f?out.right:out.left))>.1f||
            std::fabs(p.y-(p.v>.5f?out.top:out.bottom))>.1f) return false;
    }
    return true;
}
inline bool credit_count_slot(const Bounds& b,unsigned width,unsigned height) {
    // Captured native first/second count slots. Never classify the shared font
    // elsewhere: it also supplies dialogue and other HUD text.
    return width==16&&height==32&&
        (std::fabs(b.left-518)<.5f||std::fabs(b.left-531.6f)<.5f)&&
        std::fabs(b.right-b.left-13.6f)<.5f&&
        std::fabs(b.top-427)<.5f&&std::fabs(b.bottom-454.2f)<.5f;
}
inline bool aim_cursor_quad(const void* data,unsigned count) {
    if(!data||count!=4) return false;
    auto p=static_cast<const Vertex*>(data);unsigned corners=0;float left=0,top=0;
    for(unsigned i=0;i<4;++i) {
        if(!std::isfinite(p[i].x)||!std::isfinite(p[i].y)||!std::isfinite(p[i].z)||!std::isfinite(p[i].rhw)||
            !std::isfinite(p[i].u)||!std::isfinite(p[i].v)||std::fabs(p[i].z-.20002f)>.0002f||std::fabs(p[i].rhw-1)>.0002f) return false;
        bool right=std::fabs(p[i].u-1)<.0001f,upper=std::fabs(p[i].v-1)<.0001f;
        if((!right&&std::fabs(p[i].u)>.0001f)||(!upper&&std::fabs(p[i].v)>.0001f)) return false;
        float x=p[i].x-(right?32.0f:0),y=p[i].y-(upper?0:32.0f);
        if(i==0){left=x;top=y;}
        if(std::fabs(x-left)>.1f||std::fabs(y-top)>.1f) return false;
        corners|=1u<<((right?1u:0u)+(upper?2u:0u));
    }
    // Content identity distinguishes the cursor from other sprites. Do not
    // require this draw to match live aim: native coordinates can lag input.
    return corners==15;
}
class PromptFilter {
    uint32_t frame=0,credits_frame=0;bool frame_valid=false,credits_valid=false;Bounds credits={};
public:
    void begin(uint32_t next) {
        // Native text ordering varies: the count can precede its label. Keep a
        // recognized row for one following presentation, then expire it.
        if(credits_valid&&static_cast<uint32_t>(next-credits_frame)>1) credits_valid=false;
        frame=next;frame_valid=true;
    }
    bool hide(const void* data,unsigned count,unsigned width,unsigned height,Text text) {
        Bounds b;if(!footer_quad(data,count,b)) return false;
        float w=b.right-b.left,h=b.bottom-b.top;
        if(text==Text::CreditDigit&&credit_count_slot(b,width,height)) return true;
        if(width==128&&height==16&&text==Text::Join&&std::fabs(w/h-8)<.05f) return true;
        if(width==128&&height==32&&text==Text::Credits&&std::fabs(w/h-4)<.05f) {
            credits=b;credits_frame=frame;credits_valid=frame_valid;return true;
        }
        // The native credit label is followed by its count. Restrict its small
        // glyphs to the same row/depth and the label's final character slot.
        float ch=credits.bottom-credits.top;
        return credits_valid&&width==16&&height==32&&std::fabs(w/h-.5f)<.02f&&
            std::fabs(b.top-credits.top)<.5f&&std::fabs(b.bottom-credits.bottom)<.5f&&
            std::fabs(b.z-credits.z)<.0001f&&std::fabs(b.rhw-credits.rhw)<.0001f&&
            b.left>=credits.right-ch&&b.right<=credits.right+ch;
    }
};
}
