#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "texture_images.h"
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static void require(bool ok,const char* label){++checks;if(!ok){fprintf(stderr,"FAIL %s\n",label);exit(1);}}
int wmain(int argc,wchar_t** argv) {
    if(argc<2||argc>3) return 2;
    using namespace hotd2_texture;
    if(argc==3) {
        // Procedural diagnostic marker proves high-resolution replacement, not an HD art asset.
        Image image;require(read_png(argv[1],image),"source PNG decode");
        unsigned width=image.width,height=image.height;image.width*=4;image.height*=4;
        image.bgra.resize(static_cast<size_t>(image.width)*image.height*4);
        for(unsigned y=0;y<image.height;++y) for(unsigned x=0;x<image.width;++x) {
            auto out=image.bgra.data()+(static_cast<size_t>(y)*image.width+x)*4;
            bool stripe=(x/16+y/16)%2!=0;out[0]=stripe?255:20;out[1]=stripe?20:255;out[2]=255;out[3]=255;
        }
        require(replacement_size(image.width,image.height,width,height),"diagnostic 4x dimensions");
        require(write_png(argv[2],image),"diagnostic PNG saved");return 0;
    }
    Image image;image.width=image.height=2;
    image.bgra={0,0,255,255,0,255,0,128,255,0,0,0,3,17,221,64};
    auto key=content_key(image);require(key.size()==64,"stable SHA-256 key");
    require(content_key(image)==key,"same content shares key");
    require(write_png(argv[1],image),"WIC PNG write");Image decoded;
    require(read_png(argv[1],decoded)&&decoded.width==2&&decoded.height==2&&decoded.bgra==image.bgra,"PNG colour and graded alpha roundtrip");
    decoded.bgra[3]=1;require(content_key(decoded)!=key,"alpha changes identity");
    decoded=image;decoded.width=1;decoded.height=4;require(content_key(decoded)!=key,"shape changes identity");
    require(replacement_size(512,256,128,64),"4x rectangular replacement accepted");
    require(replacement_size(256,128,128,64),"2x replacement accepted");
    require(replacement_size(1024,512,128,64),"8x replacement accepted");
    require(!replacement_size(512,512,128,64),"wrong aspect rejected");
    require(!replacement_size(384,192,128,64),"unsupported 3x rejected");
    require(!replacement_size(8192,4096,128,64),"oversized replacement rejected");
    require(!replacement_size(512,256,0,64),"invalid source rejected");
    require(!valid_size(0,64)&&!valid_size(5000,5000),"invalid dimensions rejected");
    Image invalid=image;invalid.bgra.pop_back();require(content_key(invalid).empty(),"truncated image rejected");
    require(!write_png(argv[1],invalid),"invalid image cannot overwrite PNG");
    require(!read_png(L"missing-hotd2-test-file.png",decoded),"missing file rejected");
    require(valid_rows(image.bgra.data(),8,2,2,4),"packed rows accepted");
    require(valid_rows(image.bgra.data(),-16,2,2,4),"padded negative pitch accepted");
    require(!valid_rows(nullptr,8,2,2,4),"null lock rejected");
    require(!valid_rows(image.bgra.data(),4,2,2,4),"short pitch rejected");
    require(!valid_rows(image.bgra.data(),LONG_MIN,2,2,4),"unbounded pitch rejected");
    require(!valid_rows(image.bgra.data(),8,2,2,3),"unsupported pixel layout rejected");
    Image artwork;artwork.width=artwork.height=19;artwork.bgra.resize(19*19*4,255);
    for(size_t i=0;i<artwork.bgra.size();i+=4) {artwork.bgra[i]=21;artwork.bgra[i+1]=57;artwork.bgra[i+2]=93;}
    require(write_png(argv[1],artwork),"nonstandard opaque artwork saved");
    require(read_replacement(argv[1],2,2,decoded)&&decoded.width==16&&decoded.height==16&&decoded.bgra[0]==21&&decoded.bgra[3]==255,"opaque artwork imported without crop at 8x");
    require(!read_replacement(argv[1],2,4,decoded),"wrong-aspect artwork cannot be fitted");
    artwork.bgra[3]=64;require(write_png(argv[1],artwork)&&!read_replacement(argv[1],2,2,decoded),"nonstandard alpha artwork retained as native");
    printf("PASS %u texture image checks\n",checks);return 0;
}
