#!/usr/bin/env python

from __future__ import print_function

lines = [ l for l in open("rtperrors.h").readlines() if "ERR_RTP" in l ]
tgtcode = 0

defines = [ ]

for l in lines:
    tgtcode -= 1
    l = l.strip()
    parts = l.split()
    n = parts[1]
    code = int(parts[2])

    if not n.startswith("ERR_RTP"):
        raise Exception("Unexpected line: " + l)
    if tgtcode != code:
        print("WARNING: mismatch in error code for line (expected {}): {}".format(tgtcode, l))

    defines.append([ n, tgtcode])

maxlen = 0
for n,c in defines:
    maxlen = max(maxlen,len(n + " "))

boundary = 8
if maxlen%boundary != 0:
    maxlen = ((maxlen//boundary)+1)*boundary

for n,c in defines:
    print("#define {} {} {}".format(n, " "*(maxlen-len(n)), c))
