#pragma once
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace hotd2_hud {
struct Vertex {float x,y,z,rhw;uint32_t diffuse,specular;float u,v;};
static_assert(sizeof(Vertex)==32,"Observed native RHW stride");
struct Bounds {float left=0,right=0,top=0,bottom=0,z=0,rhw=0;};
enum class Text {Other,Join,Credits,CreditDigit};
inline Text text_identity(const char* key) {
    if(!key) return Text::Other;
    if(!std::strcmp(key,"3666b222f8a0a64b862c4597e5ba4f8300ba3e283bcc6616eebdbee9d30589a2")) return Text::Join;
    if(!std::strcmp(key,"e2ee80a79481fe8dea2918bb29044f975a434f8b2edd93d11c550eede02c43e8")) return Text::Credits;
    if(!std::strcmp(key,"2c3169048b14d83c03789820105940798e1096be9f7d709476ad643eaa5e6f41")) return Text::CreditDigit;
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
