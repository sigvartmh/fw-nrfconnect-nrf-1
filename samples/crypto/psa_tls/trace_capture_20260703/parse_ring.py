#!/usr/bin/env python3
"""Parse a probe-rs b32 dump of psa_call_trace_ring (RAM ring tracer)."""
import sys
import struct

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

words = []
for tok in open(sys.argv[1]).read().split():
    try:
        words.append(int(tok, 16))
    except ValueError:
        pass

data = struct.pack("<%dI" % len(words), *words)
magic, pos, size, rec_size = struct.unpack_from("<4I", data, 0)
print("magic=%08x pos=%d size=%d rec_size=%d" % (magic, pos, size, rec_size))
assert magic == 0x50534152, "bad magic"

recs = []
for i in range(min(pos, size)):
    off = 16 + i * rec_size
    (seq, cyc, key, alg, oph, fnid, n_in, n_out) = struct.unpack_from("<5IHBB", data, off)
    iva0, iva1, ivl0, ivl1, ova0, ova1, ovl0, ovl1, st = struct.unpack_from("<8Ii", data, off + 24)
    recs.append(dict(seq=seq, cyc=cyc, key=key, alg=alg, op=oph, fn=fnid,
                     n_in=n_in, n_out=n_out, iv=[(iva0, ivl0), (iva1, ivl1)],
                     ov=[(ova0, ovl0), (ova1, ovl1)], st=st))

recs.sort(key=lambda r: r["seq"])
prev_cyc = None
start = int(sys.argv[2]) if len(sys.argv) > 2 else 0
for r in recs:
    if r["seq"] < start:
        continue
    dt = (r["cyc"] - prev_cyc) & 0xFFFFFFFF if prev_cyc is not None else 0
    prev_cyc = r["cyc"]
    name = SID.get(r["fn"], "UNK_%04x" % r["fn"])
    ivs = " ".join("i@%08x:%d" % (a, l) for a, l in r["iv"] if l)
    ovs = " ".join("o@%08x:%d" % (a, l) for a, l in r["ov"] if l)
    st = "IN-FLIGHT ***" if r["st"] == 0x7fffffff else str(r["st"])
    print("%5d dt=%-8d %-20s key=%08x alg=%08x op=%d %s %s | st=%s"
          % (r["seq"], dt, name, r["key"], r["alg"], r["op"], ivs, ovs, st))
