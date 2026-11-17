// Optional bridge to HeadsetControl (github.com/Sapd/HeadsetControl), which supports many more
// headsets than the built-in drivers. If headsetcontrol.exe sits next to WirelessBattery.exe or is
// on PATH, it is run with `-o json` and any headset not already handled here is added.
#include "drivers.h"

#include <cstdlib>
#include <map>
#include <memory>
#include <string>

namespace {

// ---- minimal JSON reader (objects, arrays, strings, numbers, literals) -----------------------

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    double num = 0;
    std::string str;
    std::vector<Json> arr;
    std::map<std::string, Json> obj;
    const Json* get(const char* k) const {
        auto it = obj.find(k);
        return it == obj.end() ? nullptr : &it->second;
    }
};

struct Parser {
    const char* p;
    const char* e;
    void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) p++; }
    bool parse(Json& v, int depth = 0) {
        if (depth > 32) return false;
        ws();
        if (p >= e) return false;
        if (*p == '{') {
            v.type = Json::Obj;
            p++;
            ws();
            if (p < e && *p == '}') { p++; return true; }
            for (;;) {
                Json key;
                ws();
                if (p >= e || *p != '"' || !parseString(key.str)) return false;
                ws();
                if (p >= e || *p++ != ':') return false;
                if (!parse(v.obj[key.str], depth + 1)) return false;
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == '}') { p++; return true; }
                return false;
            }
        }
        if (*p == '[') {
            v.type = Json::Arr;
            p++;
            ws();
            if (p < e && *p == ']') { p++; return true; }
            for (;;) {
                v.arr.emplace_back();
                if (!parse(v.arr.back(), depth + 1)) return false;
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == ']') { p++; return true; }
                return false;
            }
        }
        if (*p == '"') { v.type = Json::Str; return parseString(v.str); }
        if (*p == 't' || *p == 'f' || *p == 'n') {
            v.type = *p == 'n' ? Json::Null : Json::Bool;
            v.num = *p == 't';
            while (p < e && isalpha((unsigned char)*p)) p++;
            return true;
        }
        char* end = nullptr;
        v.num = strtod(p, &end);
        if (end == p) return false;
        v.type = Json::Num;
        p = end;
        return true;
    }
    bool parseString(std::string& out) {
        p++;  // opening quote
        while (p < e && *p != '"') {
            if (*p == '\\' && p + 1 < e) {
                p++;
                char c = *p++;
                switch (c) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'u': p = p + 4 <= e ? p + 4 : e; out += '?'; break;
                    default: out += c;
                }
            } else {
                out += *p++;
            }
        }
        if (p >= e) return false;
        p++;
        return true;
    }
};

std::wstring Widen(const std::string& s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

std::wstring FindExe() {
    wchar_t self[MAX_PATH];
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir = self;
    dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    std::wstring local = dir + L"headsetcontrol.exe";
    if (GetFileAttributesW(local.c_str()) != INVALID_FILE_ATTRIBUTES) return local;
    wchar_t found[MAX_PATH];
    if (SearchPathW(nullptr, L"headsetcontrol.exe", nullptr, MAX_PATH, found, nullptr)) return found;
    return L"";
}

// Runs the command line and captures its stdout; returns false on failure or after `timeoutMs`.
bool Run(std::wstring cmd, int timeoutMs, std::string& out) {
    out.clear();
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE rd, wr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{ sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = nullptr;
    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(wr);
    if (ok) {
        ULONGLONG end = GetTickCount64() + timeoutMs;
        char buf[4096];
        for (;;) {
            if ((LONGLONG)end - (LONGLONG)GetTickCount64() <= 0) { TerminateProcess(pi.hProcess, 1); ok = FALSE; break; }
            DWORD avail = 0;
            if (!PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr)) break;  // pipe closed: process done
            if (avail) {
                DWORD n = 0;
                if (!ReadFile(rd, buf, sizeof(buf), &n, nullptr) || !n) break;
                out.append(buf, n);
                if (out.size() > (1 << 20)) { TerminateProcess(pi.hProcess, 1); ok = FALSE; break; }
                continue;
            }
            Sleep(20);
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    CloseHandle(rd);
    return ok != FALSE;
}

// One headset as last reported by HeadsetControl.
struct HscDevice { USHORT vid, pid; DeviceStatus status; };

bool Parse(const std::string& text, std::vector<HscDevice>& out) {
    size_t start = text.find('{');
    if (start == std::string::npos) return text.find_first_not_of(" \t\r\n") == std::string::npos;
    Json root;
    Parser ps{ text.c_str() + start, text.c_str() + text.size() };
    if (!ps.parse(root) || root.type != Json::Obj) return false;
    const Json* devices = root.get("devices");
    if (!devices || devices->type != Json::Arr) return true;
    for (const Json& d : devices->arr) {
        const Json* vidJ = d.get("id_vendor");
        const Json* pidJ = d.get("id_product");
        const Json* bat = d.get("battery");
        if (!vidJ || !pidJ || !bat || bat->type != Json::Obj) continue;
        HscDevice h;
        h.vid = (USHORT)strtoul(vidJ->str.c_str(), nullptr, 16);
        h.pid = (USHORT)strtoul(pidJ->str.c_str(), nullptr, 16);
        const Json* status = bat->get("status");
        const Json* level = bat->get("level");
        std::string st = status ? status->str : "";
        DeviceStatus& s = h.status;
        s.key = L"hsc:" + Hex4(h.vid) + L":" + Hex4(h.pid);
        const Json* name = d.get("device");
        s.name = name && !name->str.empty() ? Widen(name->str) : L"Headset";
        s.kind = DeviceKind::Headset;
        s.via = L"HeadsetControl";
        s.charging = st == "BATTERY_CHARGING";
        s.connected = st == "BATTERY_AVAILABLE" || st == "BATTERY_CHARGING";
        if (s.connected && level && level->type == Json::Num && level->num >= 0) s.percent = Clamp100((int)level->num);
        out.push_back(h);
    }
    return true;
}

// HeadsetControl can take several seconds per headset, so it runs on its own thread and the regular
// poll uses its latest good result, keeping it if a refresh fails. The first poll waits at most five
// seconds for the background refresh; later polls pick up its result. Only the battery is requested
// (-b), with a short HID timeout so a sleeping headset cannot stall it. The lock protects both the
// cached devices and the last refresh time, including failed attempts, so retries wait 50 seconds.
SRWLOCK g_lock = SRWLOCK_INIT;
std::vector<HscDevice> g_devices;
ULONGLONG g_updated = 0;
LONG g_running = 0;
constexpr DWORD kFirstPollWaitMs = 5000;

std::wstring Command() {
    static std::wstring exe = FindExe();
    if (exe.empty()) return L"";
    std::wstring cmd = L"\"" + exe + L"\" -b -o json --timeout 1500";
    wchar_t test[8];
    if (GetEnvironmentVariableW(L"WB_HSC_TEST_DEVICE", test, 8)) cmd += L" --test-device";  // for development
    return cmd;
}

void Refresh() {
    std::wstring cmd = Command();
    std::string out;
    std::vector<HscDevice> found;
    bool ok = !cmd.empty() && Run(cmd, 30000, out) && Parse(out, found);
    AcquireSRWLockExclusive(&g_lock);
    if (ok) g_devices = found;
    g_updated = GetTickCount64();
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI RefreshThread(LPVOID) {
    Refresh();
    InterlockedExchange(&g_running, 0);
    return 0;
}

}  // namespace

void PollHeadsetControl(PollContext& ctx) {
    if (Command().empty()) return;
    AcquireSRWLockShared(&g_lock);
    ULONGLONG updated = g_updated;
    ReleaseSRWLockShared(&g_lock);
    ULONGLONG now = GetTickCount64();
    if ((updated == 0 || now - updated > 50000) && InterlockedCompareExchange(&g_running, 1, 0) == 0) {
        HANDLE t = CreateThread(nullptr, 0, RefreshThread, nullptr, 0, nullptr);
        if (t) {
            if (updated == 0) WaitForSingleObject(t, kFirstPollWaitMs);
            CloseHandle(t);
        }
        else InterlockedExchange(&g_running, 0);
    }
    AcquireSRWLockShared(&g_lock);
    std::vector<HscDevice> devices = g_devices;
    ReleaseSRWLockShared(&g_lock);
    for (const HscDevice& h : devices)
        if (!ctx.IsClaimed(h.vid, h.pid)) ctx.Add(h.status, h.vid, h.pid);
}
