#!/usr/bin/env python3
"""Adds the battery reply (raw HID A7 31) to Keychron's official Q6 Ultra 8K ANSI firmware v1.0.4.

  python q6_ultra_8k_patch.py OFFICIAL.bin PATCHED.bin

76 bytes change: a 4-byte jump in the Launcher command handler, 40 bytes of new code in place of a
factory-only radio test routine, and the SHA-256 in the image header. The assembly is in
q6_ultra_8k_battery.S. Plain Python 3, no packages, no device access.

Any input other than the official v1.0.4 image is refused, and the result must be byte-for-byte the
image that was tested on a real keyboard, or nothing is written.
"""
import hashlib
import sys

OFFICIAL_SHA256 = "d32077e67316f756e41ab97279aaa66b9eb05fd5ca27df2422a6df56fc081e15"
PATCHED_SHA256 = "f0e6829167deee3563b3463ea0a1420b26a4ba8d3d91b83a510ea507214b0c7d"
MODEL = b"KCZKQ68K"  # at file offset 0x208
# (file offset, official bytes, patched bytes); file offset 0 is address 0x0402D000.
CHANGES = [
    # kc_user_cmd, 0x0404A73E: "cmp r3,#13 ; bhi.n exit" -> "b.w cave_entry"
    (0x1D73E, "0d2b12d8", "06f048bb"),
    # single_tone_test_start, 0x04050DD0: becomes "bx lr", followed by the A7 31 handler
    (0x23DD0, "08b501280a4b88bf0238d3f81c3b88bfc0b2074b00f52a40c3f81c0b002164201ef0d2f9bde80840",
              "70470d2b01d8f9f7b4bc302b05d1044b1b786370034b1b78a370f9f7bdbc00bf81e912004d941000"),
]
HASH_AT, HASHED_FROM = 0x180, 0x1A0  # header field: SHA-256 of everything from 0x1A0 to the end


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    src, dst = argv
    with open(src, "rb") as f:
        d = bytearray(f.read())
    if hashlib.sha256(d).hexdigest() != OFFICIAL_SHA256:
        sys.exit("Refused: %s is not the official Q6 Ultra 8K ANSI v1.0.4 image (expected sha256 %s)." % (src, OFFICIAL_SHA256))
    assert d[0x208:0x208 + len(MODEL)] == MODEL
    assert bytes(d[HASH_AT:HASHED_FROM]) == hashlib.sha256(d[HASHED_FROM:]).digest()
    for offset, old, new in CHANGES:
        old, new = bytes.fromhex(old), bytes.fromhex(new)
        assert len(old) == len(new) and bytes(d[offset:offset + len(old)]) == old
        d[offset:offset + len(new)] = new
    d[HASH_AT:HASHED_FROM] = hashlib.sha256(d[HASHED_FROM:]).digest()
    if hashlib.sha256(d).hexdigest() != PATCHED_SHA256:
        sys.exit("Refused: the result is not the tested image. Nothing was written.")
    with open(dst, "wb") as f:
        f.write(d)
    print("wrote %s (%d bytes, sha256 %s)" % (dst, len(d), PATCHED_SHA256))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
