#!/usr/bin/env python3
"""Parse psa_call trace from the instrumented psa_tls console log."""
import re
import sys

GROUPS = {
    1: ["GENERATE_RANDOM"],
    2: ["GET_KEY_ATTRIBUTES", "ABANDONED_OPEN_KEY", "ABANDONED_CLOSE_KEY",
        "IMPORT_KEY", "DESTROY_KEY", "EXPORT_KEY", "EXPORT_PUBLIC_KEY",
        "PURGE_KEY", "COPY_KEY", "GENERATE_KEY"],
    3: ["HASH_COMPUTE", "HASH_COMPARE", "HASH_SETUP", "HASH_UPDATE",
        "HASH_CLONE", "HASH_FINISH", "HASH_VERIFY", "HASH_ABORT", "CAN_DO_HASH"],
    4: ["MAC_COMPUTE", "MAC_VERIFY", "MAC_SIGN_SETUP", "MAC_VERIFY_SETUP",
        "MAC_UPDATE", "MAC_SIGN_FINISH", "MAC_VERIFY_FINISH", "MAC_ABORT"],
    5: ["CIPHER_ENCRYPT", "CIPHER_DECRYPT", "CIPHER_ENCRYPT_SETUP",
        "CIPHER_DECRYPT_SETUP", "CIPHER_GENERATE_IV", "CIPHER_SET_IV",
        "CIPHER_UPDATE", "CIPHER_FINISH", "CIPHER_ABORT", "CAN_DO_CIPHER"],
    6: ["AEAD_ENCRYPT", "AEAD_DECRYPT", "AEAD_ENCRYPT_SETUP", "AEAD_DECRYPT_SETUP",
        "AEAD_GENERATE_NONCE", "AEAD_SET_NONCE", "AEAD_SET_LENGTHS",
        "AEAD_UPDATE_AD", "AEAD_UPDATE", "AEAD_FINISH", "AEAD_VERIFY", "AEAD_ABORT"],
    7: ["SIGN_MESSAGE", "VERIFY_MESSAGE", "SIGN_HASH", "VERIFY_HASH"],
    8: ["ASYM_ENCRYPT", "ASYM_DECRYPT"],
    9: ["RAW_KEY_AGREEMENT", "KD_SETUP", "KD_GET_CAPACITY", "KD_SET_CAPACITY",
        "KD_INPUT_BYTES", "KD_INPUT_KEY", "KD_INPUT_INTEGER", "KD_KEY_AGREEMENT",
        "KD_OUTPUT_BYTES", "KD_OUTPUT_KEY", "KD_ABORT"],
    10: ["PAKE_SETUP", "PAKE_SET_ROLE", "PAKE_SET_USER", "PAKE_SET_PEER",
         "PAKE_SET_CONTEXT", "PAKE_OUTPUT", "PAKE_INPUT", "PAKE_GET_SHARED_KEY",
         "PAKE_ABORT"],
    11: ["WRAP", "UNWRAP"],
}

SID = {}
for g, names in GROUPS.items():
    for i, n in enumerate(names):
        SID[(g << 8) | i] = n


def fn_name(fid):
    return SID.get(fid, "UNKNOWN_%04x" % fid)


ansi = re.compile(r"\x1b\[[0-9;]*m|\r")

calls = {}
order = []

cur = None
for raw in open(sys.argv[1], errors="replace"):
    line = ansi.sub("", raw).strip()
    m = re.match(r"PSAC (\d+) h=(\w+) t=(-?\d+) il=(\d+) ol=(\d+)", line)
    if m:
        cur = {"seq": int(m.group(1)), "h": m.group(2), "iv": [], "ov": [],
               "ret": None, "ols": ""}
        calls[cur["seq"]] = cur
        order.append(cur)
        continue
    m = re.match(r"IV(\d+) @(\w+) l=(\d+) ?(\w*)", line)
    if m and cur:
        cur["iv"].append((m.group(2), int(m.group(3)), m.group(4)))
        continue
    m = re.match(r"OV(\d+) @(\w+) l=(\d+)", line)
    if m and cur:
        cur["ov"].append((m.group(2), int(m.group(3))))
        continue
    m = re.match(r"RET (\d+) st=(-?\d+)(.*)", line)
    if m:
        seq = int(m.group(1))
        if seq in calls:
            calls[seq]["ret"] = int(m.group(2))
            calls[seq]["ols"] = m.group(3).strip()

start = int(sys.argv[2]) if len(sys.argv) > 2 else 0
for c in order:
    if c["seq"] < start:
        continue
    if not c["iv"]:
        info = "?"
        fid = 0
    else:
        hexs = c["iv"][0][2]
        fid = int(hexs[82:84] + hexs[80:82], 16) if len(hexs) >= 84 else 0
        key_id = hexs[6:8] + hexs[4:6] + hexs[2:4] + hexs[0:2] if len(hexs) >= 8 else ""
        alg = hexs[14:16] + hexs[12:14] + hexs[10:12] + hexs[8:10] if len(hexs) >= 16 else ""
        oph = hexs[22:24] + hexs[20:22] + hexs[18:20] + hexs[16:18] if len(hexs) >= 24 else ""
        info = "key=%s alg=%s op=%s" % (key_id, alg, oph)
    ivs = " ".join("i@%s:%d" % (a, l) for a, l, _ in c["iv"][1:])
    ovs = " ".join("o@%s:%d" % (a, l) for a, l in c["ov"])
    ret = ("st=%d %s" % (c["ret"], c["ols"])) if c["ret"] is not None else "*** NO RET ***"
    print("%4d %-22s %s %s %s | %s" % (c["seq"], fn_name(fid), info, ivs, ovs, ret))
