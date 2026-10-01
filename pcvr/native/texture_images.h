#pragma once
#include <windows.h>
#include <wincodec.h>
#include <bcrypt.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

namespace hotd2_texture {
using Microsoft::WRL::ComPtr;
struct Image {unsigned width=0,height=0;std::vector<BYTE> bgra;};
inline bool valid_size(unsigned width,unsigned height) {
    return width&&height&&width<=4096&&height<=4096&&uint64_t(width)*height<=16777216;
}
inline bool valid_rows(const void* pixels,long pitch,unsigned width,unsigned height,unsigned bytes_per_pixel) {
    if(!pixels||!valid_size(width,height)||(bytes_per_pixel!=2&&bytes_per_pixel!=4)) return false;
    int64_t stride=pitch; if(stride<0) stride=-stride;
    return stride>=int64_t(width)*bytes_per_pixel && stride*height<=128*1024*1024;
}
struct Apartment {
    HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    ~Apartment(){if(SUCCEEDED(result)) CoUninitialize();}
    bool ready()const{return SUCCEEDED(result)||result==RPC_E_CHANGED_MODE;}
};
inline bool read_png(const wchar_t* path,Image& image) {
    Apartment apartment;if(!apartment.ready()) return false;
    ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICBitmapDecoder> decoder;
    if(FAILED(factory->CreateDecoderFromFilename(path,nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))) return false;
    GUID container={};if(FAILED(decoder->GetContainerFormat(&container))||container!=GUID_ContainerFormatPng) return false;
    UINT frames=0;if(FAILED(decoder->GetFrameCount(&frames))||frames!=1) return false;
    ComPtr<IWICBitmapFrameDecode> frame;Image loaded;
    if(FAILED(decoder->GetFrame(0,&frame))||FAILED(frame->GetSize(&loaded.width,&loaded.height))||!valid_size(loaded.width,loaded.height)) return false;
    ComPtr<IWICFormatConverter> converter;
    if(FAILED(factory->CreateFormatConverter(&converter))||FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return false;
    loaded.bgra.resize(static_cast<size_t>(loaded.width)*loaded.height*4);
    if(FAILED(converter->CopyPixels(nullptr,loaded.width*4,static_cast<UINT>(loaded.bgra.size()),loaded.bgra.data()))) return false;
    image=std::move(loaded);return true;
}
inline bool write_png(const wchar_t* path,const Image& image) {
    if(!valid_size(image.width,image.height)||image.bgra.size()!=static_cast<size_t>(image.width)*image.height*4) return false;
    Apartment apartment;if(!apartment.ready()) return false;
    ComPtr<IWICImagingFactory> factory;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) return false;
    ComPtr<IWICStream> stream;ComPtr<IWICBitmapEncoder> encoder;ComPtr<IWICBitmapFrameEncode> frame;
    if(FAILED(factory->CreateStream(&stream))||FAILED(stream->InitializeFromFilename(path,GENERIC_WRITE))||
        FAILED(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder))||FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache))||
        FAILED(encoder->CreateNewFrame(&frame,nullptr))||FAILED(frame->Initialize(nullptr))||FAILED(frame->SetSize(image.width,image.height))) return false;
    GUID format=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(frame->SetPixelFormat(&format))||format!=GUID_WICPixelFormat32bppBGRA||
        FAILED(frame->WritePixels(image.height,image.width*4,static_cast<UINT>(image.bgra.size()),const_cast<BYTE*>(image.bgra.data())))||
        FAILED(frame->Commit())||FAILED(encoder->Commit())) return false;
    return true;
}
inline std::string content_key(const Image& image) {
    if(!valid_size(image.width,image.height)||image.bgra.size()!=static_cast<size_t>(image.width)*image.height*4) return {};
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return {};
    DWORD bytes=0,received=0;std::string result;
    if(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<BYTE*>(&bytes),sizeof(bytes),&received,0)>=0) {
        std::vector<BYTE> object(bytes);BYTE digest[32]={};
        if(BCryptCreateHash(algorithm,&hash,object.data(),bytes,nullptr,0,0)>=0) {
            unsigned header[]={image.width,image.height}; // Version 1: LE width/height followed by top-down BGRA.
            if(BCryptHashData(hash,reinterpret_cast<BYTE*>(header),sizeof(header),0)>=0&&
                BCryptHashData(hash,const_cast<BYTE*>(image.bgra.data()),static_cast<ULONG>(image.bgra.size()),0)>=0&&
                BCryptFinishHash(hash,digest,sizeof(digest),0)>=0) {
                static const char hex[]="0123456789abcdef";
                for(BYTE byte:digest) {result+=hex[byte>>4];result+=hex[byte&15];}
            }
            BCryptDestroyHash(hash);
        }
    }
    BCryptCloseAlgorithmProvider(algorithm,0);return result;
}
inline bool replacement_size(unsigned width,unsigned height,unsigned original_width,unsigned original_height) {
    if(!valid_size(width,height)||!original_width||!original_height||width%original_width||height%original_height) return false;
    unsigned scale=width/original_width;
    return scale==height/original_height&&(scale==1||scale==2||scale==4||scale==8);
}
inline bool read_replacement(const wchar_t* path,unsigned original_width,unsigned original_height,Image& image) {
    if(!read_png(path,image)) return false;
    if(replacement_size(image.width,image.height,original_width,original_height)) return true;
    // Import opaque artwork at a compatible native texture size. Keep alpha artwork exact
    // so resampling cannot change cutout edges or introduce colour fringes.
    if(!original_width||!original_height||uint64_t(image.width)*original_height!=uint64_t(image.height)*original_width||
        image.width<original_width||image.height<original_height||image.width>uint64_t(original_width)*16) return false;
    for(size_t i=3;i<image.bgra.size();i+=4) if(image.bgra[i]!=255) return false;
    unsigned scale=8;while(scale>1&&(original_width*scale>image.width||original_height*scale>image.height)) scale/=2;
    Apartment apartment;if(!apartment.ready()) return false;
    ComPtr<IWICImagingFactory> factory;ComPtr<IWICBitmap> bitmap;ComPtr<IWICBitmapScaler> scaler;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))||
        FAILED(factory->CreateBitmapFromMemory(image.width,image.height,GUID_WICPixelFormat32bppBGRA,image.width*4,
            static_cast<UINT>(image.bgra.size()),image.bgra.data(),&bitmap))||FAILED(factory->CreateBitmapScaler(&scaler))||
        FAILED(scaler->Initialize(bitmap.Get(),original_width*scale,original_height*scale,WICBitmapInterpolationModeFant))) return false;
    Image compatible;compatible.width=original_width*scale;compatible.height=original_height*scale;
    compatible.bgra.resize(static_cast<size_t>(compatible.width)*compatible.height*4);
    if(FAILED(scaler->CopyPixels(nullptr,compatible.width*4,static_cast<UINT>(compatible.bgra.size()),compatible.bgra.data()))) return false;
    image=std::move(compatible);return true;
}
}
