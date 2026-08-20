#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "stdinclude.hpp"
#include "scgui/scGUILoop.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
    void TrimSystemMessage(char* message, DWORD& messageLength)
    {
        while (messageLength > 0 && (message[messageLength - 1] == '\r' || message[messageLength - 1] == '\n'))
        {
            message[--messageLength] = '\0';
        }
    }

    void PrintWin32Error(const char* operation, DWORD error)
    {
        char message[512]{};
        DWORD messageLength = ::FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            error,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            message,
            static_cast<DWORD>(sizeof(message)),
            NULL);
        TrimSystemMessage(message, messageLength);

        if (messageLength > 0)
            printf("[GUI][Win32] %s: error=%lu (0x%08lX), message=\"%s\"\n", operation, error, error, message);
        else
            printf("[GUI][Win32] %s: error=%lu (0x%08lX), no system message available\n", operation, error, error);
    }

    void PrintHResult(const char* operation, HRESULT result)
    {
        char message[512]{};
        const DWORD resultCode = static_cast<DWORD>(result);
        DWORD messageLength = ::FormatMessageA(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            resultCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            message,
            static_cast<DWORD>(sizeof(message)),
            NULL);
        TrimSystemMessage(message, messageLength);

        if (messageLength > 0)
            printf("[GUI][D3D11] %s: hr=0x%08lX, message=\"%s\"\n", operation, resultCode, message);
        else
            printf("[GUI][D3D11] %s: hr=0x%08lX, no system message available\n", operation, resultCode);
    }
}

bool guiDone = true;

bool attachToGame = false;

HWND hwnd;
RECT cacheRect{ 100, 100, 730, 910 };

void SetGuiDone(bool isDone) {
    guiDone = isDone;
}

void SetWindowTop()
{
    ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

void CancelWindowTop()
{
    ::SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
}

bool lastTopStat = false;
bool nowTopStat = false;
void changeTopState() {
    if (lastTopStat == nowTopStat) return;
    lastTopStat = nowTopStat;
    if (nowTopStat) {
        SetWindowTop();
    }
    else {
        CancelWindowTop();
    }

}

bool getUmaGuiDone() {
    return guiDone;
}

// Main code
void guimain()
{
    printf("[GUI] guimain entered: threadId=%lu, attachToGame=%d\n", ::GetCurrentThreadId(), attachToGame);
    guiDone = false;
    printf("[GUI] guiDone set to false\n");
    WNDCLASSW wc;
    RECT WindowRectangle;
    HWND GameHwnd;
    int WindowWide;
    int WindowHeight;

    // GameHwnd = GetConsoleWindow();
    // if (GameHwnd) attachToGame = true;

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    if (!attachToGame) {
        printf("[GUI][Win32] using standalone window path\n");
        WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"iM@S SCSP Localify", NULL };
        printf("[GUI][Win32] RegisterClassExW begin: instance=%p, className=%ls\n", static_cast<void*>(wc.hInstance), wc.lpszClassName);
        ::SetLastError(ERROR_SUCCESS);
        const ATOM classAtom = ::RegisterClassExW(&wc);
        const DWORD registerClassError = classAtom == 0 ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] RegisterClassExW end: atom=%u\n", static_cast<unsigned int>(classAtom));
        if (classAtom == 0) {
            PrintWin32Error("RegisterClassExW failed", registerClassError);
            printf("[GUI][Path] original path continues after RegisterClassExW failure\n");
        }

        printf("[GUI][Win32] CreateWindowW begin: position=(%ld,%ld), size=(%ld,%ld)\n",
            cacheRect.left,
            cacheRect.top,
            cacheRect.right - cacheRect.left,
            cacheRect.bottom - cacheRect.top);
        ::SetLastError(ERROR_SUCCESS);
        hwnd = ::CreateWindowW(wc.lpszClassName, L"iM@S SCSP GUI", WS_OVERLAPPEDWINDOW, cacheRect.left, cacheRect.top,
            cacheRect.right - cacheRect.left, cacheRect.bottom - cacheRect.top, NULL, NULL, wc.hInstance, NULL);
        const DWORD createWindowError = hwnd == NULL ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] CreateWindowW end: hwnd=%p, IsWindow=%d\n", static_cast<void*>(hwnd), ::IsWindow(hwnd));
        if (hwnd == NULL) {
            PrintWin32Error("CreateWindowW failed", createWindowError);
            printf("[GUI][Path] original path continues after CreateWindowW failure\n");
        }
        if (nowTopStat) {
            printf("[GUI][Path] nowTopStat branch entered\n");
            SetWindowTop();
            printf("[GUI][Path] nowTopStat branch completed\n");
        }
        else {
            printf("[GUI][Path] nowTopStat branch skipped\n");
        }
    }
    else {
        printf("[GUI][Win32] using attached window path\n");
        // 注册窗体类
        // 附加到指定窗体上
        wc.cbClsExtra = NULL;
        wc.cbWndExtra = NULL;
        wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
        wc.hCursor = LoadCursor(0, IDC_ARROW);
        wc.hIcon = LoadIcon(0, IDI_APPLICATION);
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = (WNDPROC)WndProc;
        wc.lpszClassName = L" ";
        wc.lpszMenuName = L" ";
        wc.style = CS_VREDRAW | CS_HREDRAW;

        printf("[GUI][Win32] RegisterClassW begin: instance=%p, className=%ls\n", static_cast<void*>(wc.hInstance), wc.lpszClassName);
        ::SetLastError(ERROR_SUCCESS);
        const ATOM classAtom = RegisterClassW(&wc);
        const DWORD registerClassError = classAtom == 0 ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] RegisterClassW end: atom=%u\n", static_cast<unsigned int>(classAtom));
        if (classAtom == 0) {
            PrintWin32Error("RegisterClassW failed", registerClassError);
            printf("[GUI][Path] original path continues after RegisterClassW failure\n");
        }

        // 得到窗口句柄
        WindowRectangle;   
        printf("[GUI][Win32] FindWindowW begin: className=UnityWndClass, windowName=imasscprism\n");
        GameHwnd = FindWindowW(L"UnityWndClass", L"imasscprism");
        printf("[GUI][Win32] FindWindowW end: hwnd=%p, IsWindow=%d\n", static_cast<void*>(GameHwnd), ::IsWindow(GameHwnd));
        if (GameHwnd == NULL) {
            printf("[GUI][Win32] FindWindowW returned NULL; no extended error is provided by this API\n");
            printf("[GUI][Path] original path continues after FindWindowW failure\n");
        }

        printf("[GUI][Win32] GetWindowRect(game window) begin\n");
        ::SetLastError(ERROR_SUCCESS);
        const BOOL getGameWindowRectResult = GetWindowRect(GameHwnd, &WindowRectangle);
        const DWORD getGameWindowRectError = getGameWindowRectResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] GetWindowRect(game window) end: result=%d\n", getGameWindowRectResult);
        if (getGameWindowRectResult != FALSE) {
            printf("[GUI][Win32] game window rect=(%ld,%ld)-(%ld,%ld)\n",
                WindowRectangle.left,
                WindowRectangle.top,
                WindowRectangle.right,
                WindowRectangle.bottom);
        }
        else {
            PrintWin32Error("GetWindowRect(game window) failed", getGameWindowRectError);
            printf("[GUI][Path] original path continues after GetWindowRect(game window) failure\n");
        }
        WindowWide = WindowRectangle.right - WindowRectangle.left;
        WindowHeight = WindowRectangle.bottom - WindowRectangle.top;

        // 创建窗体
       //  HWND hwnd = ::CreateWindowW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW, L" ", L" ", WS_POPUP, 1, 1, WindowWide, WindowHeight, 0, 0, wc.hInstance, 0);
        printf("[GUI][Win32] CreateWindowW begin: position=(1,1), size=(%d,%d)\n", WindowWide, WindowHeight);
        ::SetLastError(ERROR_SUCCESS);
        hwnd = ::CreateWindowW(wc.lpszClassName, L"", WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_LAYERED | WS_POPUP, 1, 1, WindowWide, WindowHeight, NULL, NULL, wc.hInstance, NULL);
        const DWORD createWindowError = hwnd == NULL ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] CreateWindowW end: hwnd=%p, IsWindow=%d\n", static_cast<void*>(hwnd), ::IsWindow(hwnd));
        if (hwnd == NULL) {
            PrintWin32Error("CreateWindowW failed", createWindowError);
            printf("[GUI][Path] original path continues after CreateWindowW failure\n");
        }
        printf("[GUI][Win32] GetWindowLong(GWL_EXSTYLE) begin\n");
        ::SetLastError(ERROR_SUCCESS);
        LONG lWinStyleEx = GetWindowLong(hwnd, GWL_EXSTYLE);
        const DWORD getWindowLongError = ::GetLastError();
        printf("[GUI][Win32] GetWindowLong(GWL_EXSTYLE) end: value=0x%08lX, lastError=%lu\n",
            static_cast<DWORD>(lWinStyleEx),
            getWindowLongError);
        if (lWinStyleEx == 0 && getWindowLongError != ERROR_SUCCESS)
            PrintWin32Error("GetWindowLong(GWL_EXSTYLE) failed", getWindowLongError);
        lWinStyleEx = lWinStyleEx | WS_EX_LAYERED;

        printf("[GUI][Win32] SetWindowLong(GWL_EXSTYLE) begin: value=0x%08lX\n", static_cast<DWORD>(lWinStyleEx));
        ::SetLastError(ERROR_SUCCESS);
        const LONG previousWinStyleEx = SetWindowLong(hwnd, GWL_EXSTYLE, lWinStyleEx);
        const DWORD setWindowLongError = ::GetLastError();
        printf("[GUI][Win32] SetWindowLong(GWL_EXSTYLE) end: previousValue=0x%08lX, lastError=%lu\n",
            static_cast<DWORD>(previousWinStyleEx),
            setWindowLongError);
        if (previousWinStyleEx == 0 && setWindowLongError != ERROR_SUCCESS)
            PrintWin32Error("SetWindowLong(GWL_EXSTYLE) failed", setWindowLongError);

        printf("[GUI][Win32] SetLayeredWindowAttributes begin: flags=LWA_ALPHA\n");
        ::SetLastError(ERROR_SUCCESS);
        const BOOL setLayeredWindowAttributesResult = SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
        const DWORD setLayeredWindowAttributesError = setLayeredWindowAttributesResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] SetLayeredWindowAttributes end: result=%d\n", setLayeredWindowAttributesResult);
        if (setLayeredWindowAttributesResult == FALSE)
            PrintWin32Error("SetLayeredWindowAttributes failed", setLayeredWindowAttributesError);
        // 去掉标题栏及边框
        printf("[GUI][Win32] GetWindowLongPtr(GWL_STYLE) begin\n");
        ::SetLastError(ERROR_SUCCESS);
        LONG_PTR Style = GetWindowLongPtr(hwnd, GWL_STYLE);
        const DWORD getWindowLongPtrError = ::GetLastError();
        printf("[GUI][Win32] GetWindowLongPtr(GWL_STYLE) end: value=0x%016llX, lastError=%lu\n",
            static_cast<unsigned long long>(Style),
            getWindowLongPtrError);
        if (Style == 0 && getWindowLongPtrError != ERROR_SUCCESS)
            PrintWin32Error("GetWindowLongPtr(GWL_STYLE) failed", getWindowLongPtrError);
        Style = Style & ~WS_CAPTION & ~WS_SYSMENU & ~WS_SIZEBOX;

        printf("[GUI][Win32] SetWindowLongPtr(GWL_STYLE) begin: value=0x%016llX\n", static_cast<unsigned long long>(Style));
        ::SetLastError(ERROR_SUCCESS);
        const LONG_PTR previousStyle = SetWindowLongPtr(hwnd, GWL_STYLE, Style);
        const DWORD setWindowLongPtrError = ::GetLastError();
        printf("[GUI][Win32] SetWindowLongPtr(GWL_STYLE) end: previousValue=0x%016llX, lastError=%lu\n",
            static_cast<unsigned long long>(previousStyle),
            setWindowLongPtrError);
        if (previousStyle == 0 && setWindowLongPtrError != ERROR_SUCCESS)
            PrintWin32Error("SetWindowLongPtr(GWL_STYLE) failed", setWindowLongPtrError);
        // 至顶层窗口 最大化
        printf("[GUI][Win32] SetWindowPos(size/show) begin: size=(%d,%d)\n", WindowWide, WindowHeight);
        ::SetLastError(ERROR_SUCCESS);
        const BOOL setWindowPosSizeResult = SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, WindowWide, WindowHeight, SWP_SHOWWINDOW);
        const DWORD setWindowPosSizeError = setWindowPosSizeResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] SetWindowPos(size/show) end: result=%d\n", setWindowPosSizeResult);
        if (setWindowPosSizeResult == FALSE)
            PrintWin32Error("SetWindowPos(size/show) failed", setWindowPosSizeError);

        printf("[GUI][Win32] SetWindowPos(topmost) begin\n");
        ::SetLastError(ERROR_SUCCESS);
        const BOOL setWindowPosTopmostResult = SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        const DWORD setWindowPosTopmostError = setWindowPosTopmostResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] SetWindowPos(topmost) end: result=%d\n", setWindowPosTopmostResult);
        if (setWindowPosTopmostResult == FALSE)
            PrintWin32Error("SetWindowPos(topmost) failed", setWindowPosTopmostError);
        // 设置窗体可穿透鼠标
        // SetWindowLong(hwnd, GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED);

    }

    if (hwnd != NULL)
    {
        printf("[GUI][Path] valid HWND diagnostics branch entered\n");
        DWORD windowProcessId = 0;
        ::SetLastError(ERROR_SUCCESS);
        const DWORD windowThreadId = ::GetWindowThreadProcessId(hwnd, &windowProcessId);
        const DWORD windowThreadError = windowThreadId == 0 ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] GetWindowThreadProcessId: windowThreadId=%lu, processId=%lu, callerThreadId=%lu\n",
            windowThreadId,
            windowProcessId,
            ::GetCurrentThreadId());
        if (windowThreadId == 0)
            PrintWin32Error("GetWindowThreadProcessId failed", windowThreadError);

        RECT windowRect{};
        ::SetLastError(ERROR_SUCCESS);
        const BOOL getWindowRectResult = ::GetWindowRect(hwnd, &windowRect);
        const DWORD getWindowRectError = getWindowRectResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] GetWindowRect: result=%d, rect=(%ld,%ld)-(%ld,%ld), size=(%ld,%ld)\n",
            getWindowRectResult,
            windowRect.left,
            windowRect.top,
            windowRect.right,
            windowRect.bottom,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top);
        if (getWindowRectResult == FALSE)
            PrintWin32Error("GetWindowRect failed", getWindowRectError);

        RECT clientRect{};
        ::SetLastError(ERROR_SUCCESS);
        const BOOL getClientRectResult = ::GetClientRect(hwnd, &clientRect);
        const DWORD getClientRectError = getClientRectResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] GetClientRect: result=%d, rect=(%ld,%ld)-(%ld,%ld), size=(%ld,%ld)\n",
            getClientRectResult,
            clientRect.left,
            clientRect.top,
            clientRect.right,
            clientRect.bottom,
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top);
        if (getClientRectResult == FALSE)
            PrintWin32Error("GetClientRect failed", getClientRectError);
        printf("[GUI][Path] valid HWND diagnostics branch completed\n");
    }
    else
    {
        printf("[GUI][Path] valid HWND diagnostics branch skipped because hwnd is NULL; original path continues to CreateDeviceD3D\n");
    }

    // Initialize Direct3D
    printf("[GUI][D3D11] CreateDeviceD3D begin: hwnd=%p, IsWindow=%d\n", static_cast<void*>(hwnd), ::IsWindow(hwnd));
    if (!CreateDeviceD3D(hwnd))
    {
        printf("[GUI][D3D11] CreateDeviceD3D returned false\n");
        printf("init D3D failed\n");
        printf("[GUI][Path] D3D failure cleanup begin\n");
        CleanupDeviceD3D();
        printf("[GUI][Path] D3D failure cleanup completed\n");
        if (attachToGame) {
            printf("[GUI][Path] D3D failure UnregisterClassW branch entered\n");
            ::SetLastError(ERROR_SUCCESS);
            const BOOL unregisterClassResult = ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
            const DWORD unregisterClassError = unregisterClassResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
            printf("[GUI][Path] D3D failure UnregisterClassW branch completed: result=%d\n", unregisterClassResult);
            if (unregisterClassResult == FALSE)
                PrintWin32Error("D3D failure UnregisterClassW failed", unregisterClassError);
        }
        else {
            printf("[GUI][Path] D3D failure UnregisterClassW branch skipped\n");
        }
        printf("[GUI][Path] guimain returning after CreateDeviceD3D failure\n");
        return;
    }
    printf("[GUI][D3D11] CreateDeviceD3D returned true\n");

    if (attachToGame) {
        printf("[GUI][Path] post-D3D attachToGame branch entered\n");
        // Show the window
        printf("[GUI][Win32] SetLayeredWindowAttributes(LWA_ALPHA) begin\n");
        ::SetLastError(ERROR_SUCCESS);
        const BOOL setAlphaResult = SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
        const DWORD setAlphaError = setAlphaResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] SetLayeredWindowAttributes(LWA_ALPHA) end: result=%d\n", setAlphaResult);
        if (setAlphaResult == FALSE)
            PrintWin32Error("SetLayeredWindowAttributes(LWA_ALPHA) failed", setAlphaError);

        printf("[GUI][Win32] SetLayeredWindowAttributes(LWA_COLORKEY) begin\n");
        ::SetLastError(ERROR_SUCCESS);
        const BOOL setColorKeyResult = SetLayeredWindowAttributes(hwnd, 0, 0, LWA_COLORKEY);
        const DWORD setColorKeyError = setColorKeyResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
        printf("[GUI][Win32] SetLayeredWindowAttributes(LWA_COLORKEY) end: result=%d\n", setColorKeyResult);
        if (setColorKeyResult == FALSE)
            PrintWin32Error("SetLayeredWindowAttributes(LWA_COLORKEY) failed", setColorKeyError);
        printf("[GUI][Path] post-D3D attachToGame branch completed\n");
    }
    else {
        printf("[GUI][Path] post-D3D attachToGame branch skipped\n");
    }

    printf("[GUI][Win32] ShowWindow begin\n");
    const BOOL wasWindowVisible = ::ShowWindow(hwnd, SW_SHOW);
    printf("[GUI][Win32] ShowWindow end: wasVisible=%d, hwnd=%p, IsWindowVisible=%d\n",
        wasWindowVisible,
        static_cast<void*>(hwnd),
        ::IsWindowVisible(hwnd));
    printf("[GUI][Win32] UpdateWindow begin\n");
    ::SetLastError(ERROR_SUCCESS);
    const BOOL updateWindowResult = ::UpdateWindow(hwnd);
    const DWORD updateWindowError = updateWindowResult == FALSE ? ::GetLastError() : ERROR_SUCCESS;
    printf("[GUI][Win32] UpdateWindow end: result=%d\n", updateWindowResult);
    if (updateWindowResult == FALSE) {
        if (updateWindowError != ERROR_SUCCESS)
            PrintWin32Error("UpdateWindow failed", updateWindowError);
        else
            printf("[GUI][Win32] UpdateWindow returned FALSE without an extended error\n");
        printf("[GUI][Path] original path continues after UpdateWindow returned FALSE\n");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    printf("[GUI][ImGui] CreateContext begin\n");
    ImGuiContext* imguiContext = ImGui::CreateContext();
    printf("[GUI][ImGui] CreateContext end: context=%p\n", static_cast<void*>(imguiContext));
    if (imguiContext == NULL)
        printf("[GUI][Path] original path continues after ImGui::CreateContext returned NULL\n");
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    printf("[GUI][ImGui] ImGui_ImplWin32_Init begin: hwnd=%p\n", static_cast<void*>(hwnd));
    const bool win32BackendInitResult = ImGui_ImplWin32_Init(hwnd);
    printf("[GUI][ImGui] ImGui_ImplWin32_Init end: result=%d\n", win32BackendInitResult);
    if (!win32BackendInitResult)
        printf("[GUI][Path] original path continues after ImGui_ImplWin32_Init returned false\n");
    printf("[GUI][ImGui] ImGui_ImplDX11_Init begin: device=%p, context=%p\n",
        static_cast<void*>(g_pd3dDevice),
        static_cast<void*>(g_pd3dDeviceContext));
    const bool dx11BackendInitResult = ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    printf("[GUI][ImGui] ImGui_ImplDX11_Init end: result=%d\n", dx11BackendInitResult);
    if (!dx11BackendInitResult)
        printf("[GUI][Path] original path continues after ImGui_ImplDX11_Init returned false\n");

    ImFontGlyphRangesBuilder builder;

    ImFontConfig config;
    builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
    builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
    builder.AddText("○◎△×☆");
    ImVector<ImWchar> glyphRanges;
    builder.BuildRanges(&glyphRanges);
    config.GlyphRanges = glyphRanges.Data;

    if (std::filesystem::exists("c:\\Windows\\Fonts\\msyh.ttc")) {
        printf("[GUI][Path] font selection branch: msyh.ttc\n");
        ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\msyh.ttc", 18.0f, &config);
        printf("[GUI][ImGui] AddFontFromFileTTF end: font=%p\n", static_cast<void*>(font));
        if (font == NULL)
            printf("[GUI][Path] original path continues after AddFontFromFileTTF returned NULL\n");
    }
    else {
        printf("[GUI][Path] font selection branch: segoeui.ttf\n");
        ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &config);
        printf("[GUI][ImGui] AddFontFromFileTTF end: font=%p\n", static_cast<void*>(font));
        if (font == NULL)
            printf("[GUI][Path] original path continues after AddFontFromFileTTF returned NULL\n");
    }

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 0.00f);

    // Main loop
    printf("[GUI] initialization completed; entering message/render loop\n");
    while (!guiDone)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                guiDone = true;
        }
        if (guiDone)
            break;


        if (attachToGame) {
            // 每次都将窗体置顶并跟随游戏窗体移动
            GetWindowRect(GameHwnd, &WindowRectangle);
            WindowWide = (WindowRectangle.right) - (WindowRectangle.left);
            WindowHeight = (WindowRectangle.bottom) - (WindowRectangle.top);
            DWORD dwStyle = GetWindowLong(GameHwnd, GWL_STYLE);
            if (dwStyle & WS_BORDER)
            {
                WindowRectangle.top += 23;
                WindowHeight -= 23;
            }

            // 跟随窗口移动
            MoveWindow(hwnd, WindowRectangle.left + 9, WindowRectangle.top + 9, WindowWide - 18, WindowHeight - 18, true);

        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();
        SCGUILoop::mainLoop();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        // g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    printf("[GUI] message/render loop ended; cleanup begin\n");
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    printf("[GUI] cleanup completed\n");
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    printf("[GUI][D3D11] CreateDeviceD3D entered: hwnd=%p, IsWindow=%d\n", static_cast<void*>(hWnd), ::IsWindow(hWnd));

    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    printf("[GUI][D3D11] swap chain descriptor prepared: bufferCount=%u, size=(%u,%u), format=%u, refreshRate=%u/%u, flags=0x%08X, usage=0x%08X, outputWindow=%p, sampleCount=%u, sampleQuality=%u, windowed=%d, swapEffect=%u\n",
        sd.BufferCount,
        sd.BufferDesc.Width,
        sd.BufferDesc.Height,
        static_cast<unsigned int>(sd.BufferDesc.Format),
        sd.BufferDesc.RefreshRate.Numerator,
        sd.BufferDesc.RefreshRate.Denominator,
        sd.Flags,
        sd.BufferUsage,
        static_cast<void*>(sd.OutputWindow),
        sd.SampleDesc.Count,
        sd.SampleDesc.Quality,
        sd.Windowed,
        static_cast<unsigned int>(sd.SwapEffect));

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    printf("[GUI][D3D11] createDeviceFlags=0x%08X, debugLayer=%d\n",
        createDeviceFlags,
        (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG) != 0);
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    printf("[GUI][D3D11] requested feature levels: 0x%04X, 0x%04X\n",
        static_cast<unsigned int>(featureLevelArray[0]),
        static_cast<unsigned int>(featureLevelArray[1]));

    printf("[GUI][D3D11] HARDWARE D3D11CreateDeviceAndSwapChain begin\n");
    HRESULT res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    PrintHResult("HARDWARE D3D11CreateDeviceAndSwapChain end", res);
    printf("[GUI][D3D11] HARDWARE outputs: swapChain=%p, device=%p, context=%p",
        static_cast<void*>(g_pSwapChain),
        static_cast<void*>(g_pd3dDevice),
        static_cast<void*>(g_pd3dDeviceContext));
    if (SUCCEEDED(res))
        printf(", featureLevel=0x%04X\n", static_cast<unsigned int>(featureLevel));
    else
        printf(", featureLevel unavailable\n");

    if (res == DXGI_ERROR_UNSUPPORTED) { // Try high-performance WARP software driver if hardware is not available.
        printf("[GUI][D3D11] WARP fallback entered because HARDWARE returned DXGI_ERROR_UNSUPPORTED\n");
        printf("[GUI][D3D11] WARP D3D11CreateDeviceAndSwapChain begin\n");
        res = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_WARP, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
        PrintHResult("WARP D3D11CreateDeviceAndSwapChain end", res);
        printf("[GUI][D3D11] WARP outputs: swapChain=%p, device=%p, context=%p",
            static_cast<void*>(g_pSwapChain),
            static_cast<void*>(g_pd3dDevice),
            static_cast<void*>(g_pd3dDeviceContext));
        if (SUCCEEDED(res))
            printf(", featureLevel=0x%04X\n", static_cast<unsigned int>(featureLevel));
        else
            printf(", featureLevel unavailable\n");
    }
    else {
        printf("[GUI][D3D11] WARP fallback skipped: HARDWARE hr=0x%08lX, required hr=0x%08lX\n",
            static_cast<DWORD>(res),
            static_cast<DWORD>(DXGI_ERROR_UNSUPPORTED));
    }

    if (res != S_OK)
    {
        printf("[GUI][D3D11] device/swap chain initialization failed after existing fallback logic\n");
        return false;
    }

    printf("[GUI][D3D11] CreateRenderTarget begin\n");
    CreateRenderTarget();
    printf("[GUI][D3D11] CreateRenderTarget end: renderTargetView=%p\n", static_cast<void*>(g_mainRenderTargetView));
    printf("[GUI][D3D11] CreateDeviceD3D completed successfully\n");
    return true;
}

void CleanupDeviceD3D()
{
    printf("[GUI][D3D11] CleanupDeviceD3D begin: swapChain=%p, device=%p, context=%p, renderTargetView=%p\n",
        static_cast<void*>(g_pSwapChain),
        static_cast<void*>(g_pd3dDevice),
        static_cast<void*>(g_pd3dDeviceContext),
        static_cast<void*>(g_mainRenderTargetView));
    CleanupRenderTarget();
    if (g_pSwapChain) {
        printf("[GUI][D3D11] swap chain Release branch entered\n");
        g_pSwapChain->Release();
        g_pSwapChain = NULL;
        printf("[GUI][D3D11] swap chain Release branch completed\n");
    }
    else {
        printf("[GUI][D3D11] swap chain Release branch skipped\n");
    }
    if (g_pd3dDeviceContext) {
        printf("[GUI][D3D11] device context Release branch entered\n");
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = NULL;
        printf("[GUI][D3D11] device context Release branch completed\n");
    }
    else {
        printf("[GUI][D3D11] device context Release branch skipped\n");
    }
    if (g_pd3dDevice) {
        printf("[GUI][D3D11] device Release branch entered\n");
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
        printf("[GUI][D3D11] device Release branch completed\n");
    }
    else {
        printf("[GUI][D3D11] device Release branch skipped\n");
    }
    printf("[GUI][D3D11] CleanupDeviceD3D end\n");
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    printf("[GUI][D3D11] IDXGISwapChain::GetBuffer begin\n");
    const HRESULT getBufferResult = g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    PrintHResult("IDXGISwapChain::GetBuffer end", getBufferResult);
    if (SUCCEEDED(getBufferResult))
        printf("[GUI][D3D11] back buffer pointer=%p\n", static_cast<void*>(pBackBuffer));
    else {
        printf("[GUI][D3D11] back buffer pointer unavailable because GetBuffer failed\n");
        printf("[GUI][Path] original path continues to CreateRenderTargetView after GetBuffer failure\n");
    }
    printf("[GUI][D3D11] ID3D11Device::CreateRenderTargetView begin\n");
    const HRESULT createRenderTargetViewResult = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    PrintHResult("ID3D11Device::CreateRenderTargetView end", createRenderTargetViewResult);
    printf("[GUI][D3D11] render target view pointer=%p\n", static_cast<void*>(g_mainRenderTargetView));
    if (FAILED(createRenderTargetViewResult))
        printf("[GUI][Path] original path continues to back buffer Release after CreateRenderTargetView failure\n");
    printf("[GUI][D3D11] back buffer Release begin\n");
    pBackBuffer->Release();
    printf("[GUI][D3D11] back buffer Release end\n");
}

void CleanupRenderTarget()
{
    printf("[GUI][D3D11] CleanupRenderTarget: renderTargetView=%p\n", static_cast<void*>(g_mainRenderTargetView));
    if (g_mainRenderTargetView) {
        printf("[GUI][D3D11] render target view Release branch entered\n");
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = NULL;
        printf("[GUI][D3D11] render target view Release branch completed\n");
    }
    else {
        printf("[GUI][D3D11] render target view Release branch skipped\n");
    }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SIZING: {
        RECT* rect = reinterpret_cast<RECT*>(lParam);
        cacheRect.left = rect->left;
        cacheRect.right = rect->right;
        cacheRect.top = rect->top;
        cacheRect.bottom = rect->bottom;
    }; break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}