#!/usr/bin/env python3
"""Firmware tool for the Keychron Q6 Ultra 8K ANSI (USB 3434:1260) on Windows. No third-party packages.

  python q6_ultra_8k_flash.py check  IMAGE     offline: validate an image file
  python q6_ultra_8k_flash.py info             read-only: identify the keyboard (must be on its USB cable)
  python q6_ultra_8k_flash.py battery          read-only: ask for the battery level (USB cable or 2.4 GHz receiver)
  python q6_ultra_8k_flash.py flash  IMAGE     dry run: identify the keyboard, validate the image, stop
  python q6_ultra_8k_flash.py flash  IMAGE --yes-flash-my-keyboard

Only two images are accepted: Keychron's official v1.0.4 and the same image patched by
q6_ultra_8k_patch.py. See q6_ultra_8k.md.

The update protocol is the one in Keychron's firmware source (github.com/Keychron/zmk, branch rtl8762g,
app/src/dfu/tdfu.c) and mirrors what Keychron Launcher sends: HID usage page 0x8C, output report 0xB2,
input report 0xB1, frames  AA 55|56  len  ~len  sn  opcode  data...  sum16.
The image is staged in a spare flash area; the keyboard checks CRC32, SHA-256 and the model name before
it accepts IMAGE_SWITCH, so an interrupted or corrupted transfer leaves the running firmware in place.
"""
import ctypes
import ctypes.wintypes as wt
import hashlib
import sys
import time
import zlib

VID, PID_KEYBOARD, PID_DONGLE = 0x3434, 0x1260, 0xD028
MODEL = b"KCZKQ68K"
KNOWN_IMAGES = {
    "d32077e67316f756e41ab97279aaa66b9eb05fd5ca27df2422a6df56fc081e15": "official v1.0.4 (Feb 10 2026)",
    "f0e6829167deee3563b3463ea0a1420b26a4ba8d3d91b83a510ea507214b0c7d": "official v1.0.4 + battery reply (A7 31)",
}
OUT_ID, IN_ID = 0xB2, 0xB1
OP_MODEL, OP_DFU_VER, OP_SET_SC, OP_START, OP_SEND, OP_VERIFY, OP_SWITCH, OP_BUILD = 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x6F
CHUNK = 16  # what Keychron Launcher uses

# ---------------------------------------------------------------- Win32 HID
setupapi = ctypes.WinDLL("setupapi", use_last_error=True)
hid = ctypes.WinDLL("hid", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)


class GUID(ctypes.Structure):
    _fields_ = [("d1", wt.DWORD), ("d2", wt.WORD), ("d3", wt.WORD), ("d4", ctypes.c_ubyte * 8)]


class IFDATA(ctypes.Structure):
    _fields_ = [("cbSize", wt.DWORD), ("guid", GUID), ("flags", wt.DWORD), ("reserved", ctypes.c_void_p)]


class ATTR(ctypes.Structure):
    _fields_ = [("Size", wt.ULONG), ("VendorID", wt.USHORT), ("ProductID", wt.USHORT), ("VersionNumber", wt.USHORT)]


class CAPS(ctypes.Structure):
    _fields_ = [("Usage", wt.USHORT), ("UsagePage", wt.USHORT), ("InLen", wt.USHORT), ("OutLen", wt.USHORT),
                ("FeatLen", wt.USHORT), ("Reserved", wt.USHORT * 17), ("rest", wt.USHORT * 10)]


class OVERLAPPED(ctypes.Structure):
    _fields_ = [("Internal", ctypes.c_void_p), ("InternalHigh", ctypes.c_void_p), ("Offset", wt.DWORD),
                ("OffsetHigh", wt.DWORD), ("hEvent", wt.HANDLE)]


INVALID = wt.HANDLE(-1).value
setupapi.SetupDiGetClassDevsW.restype = wt.HANDLE
setupapi.SetupDiGetClassDevsW.argtypes = [ctypes.POINTER(GUID), wt.LPCWSTR, wt.HWND, wt.DWORD]
setupapi.SetupDiEnumDeviceInterfaces.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.POINTER(GUID), wt.DWORD, ctypes.POINTER(IFDATA)]
setupapi.SetupDiGetDeviceInterfaceDetailW.argtypes = [wt.HANDLE, ctypes.POINTER(IFDATA), ctypes.c_void_p, wt.DWORD,
                                                      ctypes.POINTER(wt.DWORD), ctypes.c_void_p]
setupapi.SetupDiDestroyDeviceInfoList.argtypes = [wt.HANDLE]
k32.CreateFileW.restype = wt.HANDLE
k32.CreateFileW.argtypes = [wt.LPCWSTR, wt.DWORD, wt.DWORD, ctypes.c_void_p, wt.DWORD, wt.DWORD, wt.HANDLE]
k32.CreateEventW.restype = wt.HANDLE
k32.CreateEventW.argtypes = [ctypes.c_void_p, wt.BOOL, wt.BOOL, wt.LPCWSTR]
k32.ReadFile.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD), ctypes.POINTER(OVERLAPPED)]
k32.WriteFile.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD), ctypes.POINTER(OVERLAPPED)]
k32.GetOverlappedResult.argtypes = [wt.HANDLE, ctypes.POINTER(OVERLAPPED), ctypes.POINTER(wt.DWORD), wt.BOOL]
k32.WaitForSingleObject.argtypes = [wt.HANDLE, wt.DWORD]
k32.CancelIo.argtypes = [wt.HANDLE]
k32.CloseHandle.argtypes = [wt.HANDLE]
hid.HidD_GetAttributes.argtypes = [wt.HANDLE, ctypes.POINTER(ATTR)]
hid.HidD_GetPreparsedData.argtypes = [wt.HANDLE, ctypes.POINTER(ctypes.c_void_p)]
hid.HidD_FreePreparsedData.argtypes = [ctypes.c_void_p]
hid.HidP_GetCaps.argtypes = [ctypes.c_void_p, ctypes.POINTER(CAPS)]
hid.HidD_FlushQueue.argtypes = [wt.HANDLE]


def enum_hid(vid):
    guid = GUID()
    hid.HidD_GetHidGuid(ctypes.byref(guid))
    info = setupapi.SetupDiGetClassDevsW(ctypes.byref(guid), None, None, 0x12)
    out, i = [], 0
    while True:
        ifd = IFDATA()
        ifd.cbSize = ctypes.sizeof(ifd)
        if not setupapi.SetupDiEnumDeviceInterfaces(info, None, ctypes.byref(guid), i, ctypes.byref(ifd)):
            break
        i += 1
        need = wt.DWORD()
        setupapi.SetupDiGetDeviceInterfaceDetailW(info, ctypes.byref(ifd), None, 0, ctypes.byref(need), None)
        buf = ctypes.create_string_buffer(need.value)
        ctypes.cast(buf, ctypes.POINTER(wt.DWORD))[0] = 8
        if not setupapi.SetupDiGetDeviceInterfaceDetailW(info, ctypes.byref(ifd), buf, need, None, None):
            continue
        path = ctypes.wstring_at(ctypes.addressof(buf) + 4)
        h = k32.CreateFileW(path, 0, 3, None, 3, 0, None)
        if h == INVALID:
            continue
        attr = ATTR()
        attr.Size = ctypes.sizeof(attr)
        hid.HidD_GetAttributes(h, ctypes.byref(attr))
        if attr.VendorID == vid:
            pp, caps = ctypes.c_void_p(), CAPS()
            if hid.HidD_GetPreparsedData(h, ctypes.byref(pp)):
                hid.HidP_GetCaps(pp, ctypes.byref(caps))
                hid.HidD_FreePreparsedData(pp)
            out.append(dict(path=path, pid=attr.ProductID, page=caps.UsagePage, usage=caps.Usage, inl=caps.InLen, outl=caps.OutLen))
        k32.CloseHandle(h)
    setupapi.SetupDiDestroyDeviceInfoList(info)
    return out


class Dev:
    def __init__(self, coll):
        self.c = coll
        self.h = k32.CreateFileW(coll["path"], 0xC0000000, 3, None, 3, 0x40000000, None)
        if self.h == INVALID:
            raise OSError("cannot open the HID interface (error %d); close Keychron Launcher and other tools" % ctypes.get_last_error())
        self.ev = k32.CreateEventW(None, True, False, None)

    def close(self):
        k32.CloseHandle(self.h)
        k32.CloseHandle(self.ev)

    def flush(self):
        hid.HidD_FlushQueue(self.h)

    def write(self, data):
        buf = bytes(data).ljust(self.c["outl"], b"\0")
        ov = OVERLAPPED()
        ov.hEvent = self.ev
        n = wt.DWORD()
        if not k32.WriteFile(self.h, buf, len(buf), None, ctypes.byref(ov)):
            if ctypes.get_last_error() != 997:
                return False
            if k32.WaitForSingleObject(self.ev, 1000) != 0:
                k32.CancelIo(self.h)
                return False
        return bool(k32.GetOverlappedResult(self.h, ctypes.byref(ov), ctypes.byref(n), False))

    def read(self, timeout_ms):
        buf = ctypes.create_string_buffer(self.c["inl"])
        ov = OVERLAPPED()
        ov.hEvent = self.ev
        n = wt.DWORD()
        if not k32.ReadFile(self.h, buf, len(buf), None, ctypes.byref(ov)):
            if ctypes.get_last_error() != 997:
                return None
            if k32.WaitForSingleObject(self.ev, max(1, int(timeout_ms))) != 0:
                k32.CancelIo(self.h)
                k32.GetOverlappedResult(self.h, ctypes.byref(ov), ctypes.byref(n), True)
                return None
        if not k32.GetOverlappedResult(self.h, ctypes.byref(ov), ctypes.byref(n), False):
            return None
        return buf.raw[:n.value]


# ---------------------------------------------------------------- protocol
def frame(op, data=b"", sn=1, ack=False):
    """AA 55|56, len, ~len, sn, opcode, data, sum16(opcode + data); len counts opcode + data + the 2 sum bytes."""
    body = bytes([op]) + bytes(data)
    ln = len(body) + 2
    s = sum(body) & 0xFFFF
    f = bytes([0xAA, 0x56 if ack else 0x55, ln, (~ln) & 0xFF, sn]) + body + bytes([s & 0xFF, s >> 8])
    assert len(f) <= 32
    return f


def crc_step(crc, chunk):
    """The firmware's CRC32: reflected 0xEDB88320, initial 0xFFFFFFFF, no final inversion."""
    return zlib.crc32(chunk, crc ^ 0xFFFFFFFF) ^ 0xFFFFFFFF


def selftest():
    # Frames taken from Keychron Launcher's own code.
    assert frame(0x6E, sn=1, ack=True) == bytes([170, 86, 3, 252, 1, 110, 110, 0])
    assert frame(0x61, sn=2) == bytes([170, 85, 3, 252, 2, 97, 97, 0])
    assert frame(0x62, b"\0", sn=2) == bytes([170, 85, 4, 251, 2, 98, 0, 98, 0])
    assert frame(0x63, b"\0", sn=3, ack=True) == bytes([170, 86, 4, 251, 3, 99, 0, 99, 0])
    assert frame(0x66, sn=1) == bytes([170, 85, 3, 252, 1, 102, 102, 0])
    d = bytes(range(16))
    f = frame(0x64, d, sn=4, ack=True)
    assert f[:6] == bytes([170, 86, 19, 236, 4, 100]) and f[6:22] == d and f[22] | f[23] << 8 == 100 + sum(d)
    assert crc_step(0xFFFFFFFF, b"123456789") == 0xCBF43926 ^ 0xFFFFFFFF


class Dfu:
    def __init__(self, dev):
        self.d = dev

    def cmd(self, op, data=b"", sn=1, ack=False, timeout=1.0, expect_reply=True):
        f = frame(op, data, sn, ack)
        self.d.flush()
        if not self.d.write(bytes([OUT_ID]) + f):
            raise IOError("write failed")
        if not expect_reply:
            return None
        end = time.monotonic() + timeout
        while True:
            left = (end - time.monotonic()) * 1000
            if left <= 0:
                return None
            r = self.d.read(left)
            if r is None:
                return None
            # [B1][AA][55][len][~len][sn][rsp_cmd][ack_sn][ack_cmd][ack_status][payload...][sum16]
            if len(r) < 12 or r[0] != IN_ID or r[1] != 0xAA or r[2] != 0x55 or r[3] != (~r[4]) & 0xFF:
                continue
            if r[7] != sn or r[8] != op:
                continue
            ln = r[3]
            body = r[1:]
            if 5 + ln > 32:  # long replies continue in a second report
                more = self.d.read(500)
                if more is None or more[0] != IN_ID:
                    return None
                body += more[1:]
            body = body[:5 + ln]
            if sum(body[5:-2]) & 0xFFFF != body[-2] | body[-1] << 8:
                continue
            return dict(rsp=body[5], status=body[8], payload=body[9:-2])


def find(pid, page, usage):
    return [c for c in enum_hid(VID) if c["pid"] == pid and c["page"] == page and c["usage"] == usage]


def open_dfu():
    colls = find(PID_KEYBOARD, 0x8C, 0x01)
    if not colls:
        sys.exit("Keyboard not found on USB. Plug in the cable and set the switch to cable mode "
                 "(the 2.4 GHz receiver cannot be used for this).")
    if len(colls) > 1:
        sys.exit("More than one Q6 Ultra is connected; unplug the others.")
    return Dev(colls[0])


def text(b):
    return bytes(b).split(b"\0")[0].decode("ascii", "replace")


def read_identity(dfu):
    r = dfu.cmd(OP_MODEL, sn=1)
    if not r or r["status"] != 0 or len(r["payload"]) < 34:
        sys.exit("The keyboard did not answer the identification request.")
    p = r["payload"]
    ident = dict(model=text(p[0:10]), fw=text(p[12:22]), hw=text(p[22:32]), cmdver=p[32] | p[33] << 8)
    r = dfu.cmd(OP_DFU_VER, sn=2)
    ident["dfu"] = None if not r or len(r["payload"]) < 3 else (r["payload"][0] | r["payload"][1] << 8, r["payload"][2])
    r = dfu.cmd(OP_BUILD, sn=3)
    ident["build"] = text(r["payload"]) if r else "?"
    return ident


def load_image(path):
    d = open(path, "rb").read()
    sha = hashlib.sha256(d).hexdigest()
    problems = []
    if len(d) < 0x600 or len(d) > 0x80000:
        problems.append("implausible size %d" % len(d))
    else:
        if d[0x208:0x210] != MODEL:
            problems.append("model name in the image is %r, expected %r" % (d[0x208:0x210], MODEL))
        if hashlib.sha256(d[0x1A0:]).digest() != d[0x180:0x1A0]:
            problems.append("header SHA-256 does not match the contents")
        if int.from_bytes(d[0x1A8:0x1AC], "little") != len(d) - 0x500:
            problems.append("payload length field does not match the file size")
        if d[0x1A6:0x1A8] != bytes([0xA9, 0x37]):
            problems.append("unexpected image id")
    return d, sha, KNOWN_IMAGES.get(sha), problems


def cmd_check(path):
    d, sha, known, problems = load_image(path)
    print("file    :", path)
    print("size    : %d bytes (%d chunks)" % (len(d), (len(d) + CHUNK - 1) // CHUNK))
    print("sha256  :", sha)
    print("identity:", known or "UNKNOWN image (not one of the two this tool accepts)")
    print("checks  :", "OK" if not problems else "; ".join(problems))
    return 0 if not problems else 1


def cmd_info():
    dev = open_dfu()
    try:
        i = read_identity(Dfu(dev))
    finally:
        dev.close()
    print("model   :", i["model"], "(expected %s)" % MODEL.decode())
    print("firmware:", i["fw"], " hardware:", i["hw"], " build:", i["build"])
    print("update protocol: command version %d, dfu version/encryption %s" % (i["cmdver"], i["dfu"]))
    return 0


def cmd_battery():
    for pid, via in ((PID_KEYBOARD, "USB cable"), (PID_DONGLE, "2.4 GHz receiver")):
        colls = [c for c in find(pid, 0xFF60, 0x61) if c["outl"] == 33]
        if not colls:
            continue
        dev = Dev(colls[0])
        try:
            dev.flush()
            dev.write(bytes([0x00, 0xA7, 0x31]))
            end = time.monotonic() + 1.0
            while time.monotonic() < end:
                r = dev.read((end - time.monotonic()) * 1000)
                if r is None:
                    break
                if r[1:3] == b"\xA7\x31":
                    pct, chg = r[3], r[4]
                    if 1 <= pct <= 100:
                        print("%s: battery %d%%%s" % (via, pct, ", charging" if chg else ""))
                    else:
                        print("%s: the keyboard answered without a battery level (stock firmware, or asleep)" % via)
                    return 0
        finally:
            dev.close()
        print("%s: no answer" % via)
        return 1
    print("No Q6 Ultra (cable) or Ultra-Link 8K receiver found.")
    return 1


def cmd_flash(path, confirmed):
    data, sha, known, problems = load_image(path)
    if problems:
        sys.exit("Refusing to flash: " + "; ".join(problems))
    if not known:
        sys.exit("Refusing to flash an image this tool does not know (sha256 %s)." % sha)
    dev = open_dfu()
    dfu = Dfu(dev)
    try:
        ident = read_identity(dfu)
        print("keyboard: %s, firmware %s, build %s" % (ident["model"], ident["fw"], ident["build"]))
        print("image   : %s (%d bytes)" % (known, len(data)))
        if ident["model"].encode() != MODEL:
            sys.exit("Refusing to flash: this is not a Q6 Ultra ANSI (%s)." % ident["model"])
        if not ident["fw"].startswith("1.0.4"):
            sys.exit("Refusing to flash: the keyboard runs firmware %s. Both images are v1.0.4 and were only "
                     "tested from v1.0.4; update it with Keychron Launcher first." % ident["fw"])
        if ident["dfu"] != (1, 0):
            sys.exit("Refusing to flash: unexpected update protocol %s." % (ident["dfu"],))
        if not confirmed:
            print("Dry run: everything checks out. Add --yes-flash-my-keyboard to actually write the firmware.")
            return 0
        r = dfu.cmd(OP_SET_SC, b"\0", sn=2)
        if not r or r["status"] != 0:
            sys.exit("The keyboard rejected the transfer mode; nothing was written.")
        r = dfu.cmd(OP_START, b"\0", sn=3, ack=True, timeout=3.0)
        if not r or r["status"] != 0:
            sys.exit("The keyboard did not accept START; nothing was written.")
        total = (len(data) + CHUNK - 1) // CHUNK
        crc, sn, t0 = 0xFFFFFFFF, 4, time.monotonic()
        for n in range(total):
            chunk = data[n * CHUNK:(n + 1) * CHUNK]
            crc = crc_step(crc, chunk)
            for attempt in range(6):
                r = dfu.cmd(OP_SEND, chunk, sn=sn, ack=True, timeout=0.5)
                if r:
                    break
            if not r:
                sys.exit("\nNo answer at chunk %d of %d. Transfer aborted before activation: the keyboard keeps its "
                         "current firmware. Unplug and replug it, then try again." % (n, total))
            p = r["payload"]
            if r["status"] != 0 or len(p) < 5 or p[0] != 0:
                sys.exit("\nChunk %d refused by the keyboard. Aborted before activation; current firmware is kept." % n)
            if int.from_bytes(p[1:5], "little") != crc:
                sys.exit("\nChecksum mismatch at chunk %d. Aborted before activation; current firmware is kept." % n)
            sn = sn + 1 if sn < 255 else 1
            if n % 256 == 0 or n == total - 1:
                print("\r  sent %d / %d chunks" % (n + 1, total), end="", flush=True)
        print("\n  transfer took %.0f s; asking the keyboard to verify..." % (time.monotonic() - t0))
        c = crc.to_bytes(4, "little")
        r = dfu.cmd(OP_VERIFY, c + c, sn=1, timeout=10.0)
        if not r or r["status"] != 0 or not r["payload"] or r["payload"][0] != 0:
            code = r["payload"][0] if r and r["payload"] else None
            sys.exit("The keyboard rejected the staged image (code %s). It was NOT activated; current firmware is kept." % code)
        print("  keyboard verified CRC32, SHA-256 and model name. Activating (the keyboard restarts)...")
        dfu.cmd(OP_SWITCH, sn=1, expect_reply=False)
    finally:
        dev.close()
    for _ in range(40):
        time.sleep(1)
        if find(PID_KEYBOARD, 0x8C, 0x01):
            break
    else:
        print("The keyboard has not come back on USB after 40 s. Unplug and replug the cable.")
        return 1
    time.sleep(1)
    dev = open_dfu()
    try:
        ident = read_identity(Dfu(dev))
    finally:
        dev.close()
    print("keyboard is back: %s, firmware %s, build %s" % (ident["model"], ident["fw"], ident["build"]))
    return cmd_battery()


def main(argv):
    selftest()
    if len(argv) >= 2 and argv[0] == "check":
        return cmd_check(argv[1])
    if argv == ["info"]:
        return cmd_info()
    if argv == ["battery"]:
        return cmd_battery()
    if len(argv) >= 2 and argv[0] == "flash":
        return cmd_flash(argv[1], "--yes-flash-my-keyboard" in argv)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
