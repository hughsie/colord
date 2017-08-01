#!/usr/bin/python
# -*- coding: utf-8 -*-

import csv

blob = [None] * 1024

fn = 'huey-p70-run2.csv'

with open(fn, 'rb') as f:
    reader = csv.reader(f)

    val_53_req = 0

    # write address map
    rows = []
    for row in reader:
        rows.append(row)
    for row in reversed(rows):
        data = row[7].split(',')
        if data[0] == '00' and data[1] == '08':
            addr = (int(data[2], 16) << 8) + int(data[3], 16)
            blob[addr+0] = int(data[4], 16)
            blob[addr+1] = int(data[5], 16)
            blob[addr+2] = int(data[6], 16)
            blob[addr+3] = int(data[7], 16)

        if data[0] == '04':
            print "REQ 04 ", data[1:3]
            val = int(data[1]+data[2], 16)
            print "REQ 04 =", val
            print "factor calculated as = %.6f" % (float(val) / float(val_53_req)), "should be", 2.9 * val_53_req
        if data[0] == '53':
            val = int(data[1]+data[2], 16)
            print "REQ 53 =", val
        if data[0] == '00' and data[1] == '04':
            val = int(data[2]+data[3]+data[4]+data[5], 16)
            print "res 04 =", val,"SO LUMINANCE=%.9f" % (float(val_53_req) * float(1000) / float(val)), '\n'
        if data[0] == '00' and data[1] == '53':
            val_53_req = int(data[4]+data[5], 16)
            print "res 53 =", val_53_req

# save address map as binary
output = open(fn + '.bin', 'wb')
for b in blob:
    if b is None:
        output.write(chr(0xfe))
        continue
    output.write(chr(b))
