// language: C++17, file: main.cpp, app: priceboard — live crypto price ticker
// Windows 11, MSVC, WinAPI + WinHTTP. No external dependencies.
// A wall of live prices for popular assets, refreshed every 30 s from CoinGecko.
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

struct Coin { const wchar_t* label; const wchar_t* id; double price; double chg; };

static Coin g_coins[] = {
    { L"BTC",  L"bitcoin",    0, 0 }, { L"ETH", L"ethereum", 0, 0 },
    { L"SOL",  L"solana",     0, 0 }, { L"XRP", L"ripple",   0, 0 },
    { L"ADA",  L"cardano",    0, 0 }, { L"DOGE", L"dogecoin",0, 0 },
    { L"AVAX", L"avalanche-2",0, 0 }, { L"LINK", L"chainlink",0,0 },
    { L"DOT",  L"polkadot",   0, 0 }, { L"LTC", L"litecoin", 0, 0 },
};

static HWND g_board;

static std::string HttpsGet(const std::wstring& host, const std::wstring& path) {
    std::string out;
    HINTERNET sess = WinHttpOpen(L"priceboard/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!sess) return out;
    HINTERNET conn = WinHttpConnect(sess, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (conn) {
        HINTERNET req = WinHttpOpenRequest(conn, L"GET", path.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           WINHTTP_FLAG_SECURE);
        if (req && WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                      WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
                && WinHttpReceiveResponse(req, nullptr)) {
            DWORD bytes = 0; char buf[8192];
            for (;;) {
                if (!WinHttpReadData(req, buf, sizeof(buf), &bytes) || bytes == 0) break;
                out.append(buf, bytes);
            }
        }
        if (req) WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);
    }
    WinHttpCloseHandle(sess);
    return out;
}

// Find "<id>":{"usd":<num>,"usd_24h_change":<num>
static void Extract(const std::string& json, const std::string& id, double& usd, double& chg) {
    size_t p = json.find("\"" + id + "\":{\"usd\":");
    if (p == std::string::npos) return;
    p += id.size() + 9;
    usd = std::stod(json.substr(p, json.find_first_of(",}", p) - p));
    size_t c = json.find("\"usd_24h_change\":", p);
    if (c != std::string::npos)
        chg = std::stod(json.substr(c + 17, json.find_first_of(",}", c + 17) - (c + 17)));
}

static DWORD WINAPI Poll(LPVOID) {
    std::wstring ids;
    for (auto& c : g_coins) { if (!ids.empty()) ids += L","; ids += c.id; }
    for (;;) {
        std::string body = HttpsGet(L"api.coingecko.com",
            L"/api/v3/simple/price?ids=" + ids + L"&vs_currencies=usd&include_24hr_change=true");
        if (!body.empty()) {
            for (auto& c : g_coins) {
                std::string id(c.id, c.id + wcslen(c.id));
                Extract(body, id, c.price, c.chg);
            }
            PostMessage(g_board, WM_APP, 0, 0);
        }
        Sleep(30000);
    }
    return 0;
}

static LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: g_board = w; CreateThread(nullptr, 0, Poll, nullptr, 0, nullptr); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(w, &ps);
        SetBkMode(dc, TRANSPARENT);
        HFONT name = CreateFontW(30, 0, 0, 0, FW_BOLD, 0, 0, 0, 0, 0, 0, 0, 0, L"Consolas");
        HFONT val  = CreateFontW(30, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Consolas");
        wchar_t buf[64];
        for (int i = 0; i < 10; ++i) {
            int row = i / 2, colI = i % 2;
            int x = 30 + colI * 340, y = 30 + row * 70;
            SelectObject(dc, name); SetTextColor(dc, RGB(139, 148, 158));
            TextOutW(dc, x, y, g_coins[i].label, (int)wcslen(g_coins[i].label));
            swprintf(buf, 64, L"$%.2f", g_coins[i].price);
            SelectObject(dc, val);
            SetTextColor(dc, g_coins[i].chg >= 0 ? RGB(63, 185, 80) : RGB(248, 81, 73));
            TextOutW(dc, x + 110, y, buf, (int)wcslen(buf));
            swprintf(buf, 64, L"%+.2f%%", g_coins[i].chg);
            TextOutW(dc, x + 110, y + 32, buf, (int)wcslen(buf));
        }
        DeleteObject(name); DeleteObject(val);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_APP: InvalidateRect(w, nullptr, TRUE); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc; wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(13, 17, 23));
    wc.lpszClassName = L"PriceBoardWnd";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, L"PriceBoardWnd", L"PriceBoard — live crypto ticker",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 720, 480,
                             nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}
