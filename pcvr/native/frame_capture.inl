// Capture only the game's own render target, never the desktop or other windows.
static BYTE channel(DWORD pixel,DWORD mask) {
    if(!mask) return 0;
    unsigned shift=0; while(!(mask&1)) {mask>>=1; ++shift;}
    return static_cast<BYTE>((static_cast<unsigned long long>((pixel>>shift)&mask)*255)/mask);
}
static void capture_frame(IDirect3DDevice7* device,LONG frame) {
    IDirectDrawSurface7* surface=nullptr;
    HRESULT hr=device->GetRenderTarget(&surface);
    if(FAILED(hr)||!surface) {log_line("CAPTURE GetRenderTarget hr=%08lx",hr);return;}
    DDSURFACEDESC2 d={}; d.dwSize=sizeof(d);
    hr=surface->Lock(nullptr,&d,DDLOCK_READONLY|DDLOCK_WAIT,nullptr);
    if(FAILED(hr)) {log_line("CAPTURE Lock hr=%08lx",hr);surface->Release();return;}
    DWORD bits=d.ddpfPixelFormat.dwRGBBitCount;
    if((bits==16||bits==32)&&d.dwWidth>0&&d.dwHeight>0&&d.dwWidth<=4096&&d.dwHeight<=4096) {
        DWORD stride=(d.dwWidth*3+3)&~3u;
        std::vector<BYTE> pixels(static_cast<size_t>(stride)*d.dwHeight,0);
        for(DWORD y=0;y<d.dwHeight;++y) for(DWORD x=0;x<d.dwWidth;++x) {
            auto row=static_cast<BYTE*>(d.lpSurface)+static_cast<ptrdiff_t>(y)*d.lPitch;
            DWORD pixel=0; memcpy(&pixel,row+x*(bits/8),bits/8);
            auto dest=pixels.data()+y*stride+x*3;
            dest[0]=channel(pixel,d.ddpfPixelFormat.dwBBitMask);
            dest[1]=channel(pixel,d.ddpfPixelFormat.dwGBitMask);
            dest[2]=channel(pixel,d.ddpfPixelFormat.dwRBitMask);
        }
        wchar_t path[32768]={}; DWORD n=GetModuleFileNameW(self,path,32768);
        auto slash=wcsrchr(path,L'\\');
        if(n && n<32768 && slash && slash-path+40<32768) {
            swprintf_s(slash+1,32768-(slash+1-path),L"frame-%06ld.bmp",frame);
            BITMAPFILEHEADER fh={}; BITMAPINFOHEADER ih={};
            fh.bfType=0x4d42; fh.bfOffBits=sizeof(fh)+sizeof(ih); fh.bfSize=fh.bfOffBits+static_cast<DWORD>(pixels.size());
            ih.biSize=sizeof(ih); ih.biWidth=d.dwWidth; ih.biHeight=-static_cast<LONG>(d.dwHeight);
            ih.biPlanes=1; ih.biBitCount=24; ih.biSizeImage=static_cast<DWORD>(pixels.size());
            FILE* file=nullptr;
            if(_wfopen_s(&file,path,L"wb")==0&&file) {
                bool ok=fwrite(&fh,sizeof(fh),1,file)==1 && fwrite(&ih,sizeof(ih),1,file)==1 &&
                    fwrite(pixels.data(),pixels.size(),1,file)==1;
                fclose(file); log_line("CAPTURE frame=%ld size=%lux%lu file=%ls success=%d",frame,d.dwWidth,d.dwHeight,path,ok);
            }
        }
    } else log_line("CAPTURE unsupported bits=%lu size=%lux%lu",bits,d.dwWidth,d.dwHeight);
    surface->Unlock(nullptr); surface->Release();
}
