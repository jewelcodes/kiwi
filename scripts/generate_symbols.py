#!/usr/bin/env python3

import sys

if(len(sys.argv) < 2):
    print("/* empty symbol file */")
    print("#include <kiwi/debug.h>")
    print()
    print("KernelSymbol __ksyms[] = {};")
    print("usize __ksyms_count = 0;")
    sys.exit(0)

for file in sys.argv[1:]:
    text_symbols = [
        {
            "name": line.split()[2],
            "address": line.split()[0]
        }
        for line in open(file).readlines()
        if line.split()[1] in ("T", "t")
    ]

    print(f"/* generated from {file} */")
    print("#include <kiwi/debug.h>")
    print()
    print("KernelSymbol __ksyms[] = {")

    for sym in text_symbols:
        print("    {")
        print(f"        .name = \"{sym['name']}\",")
        print(f"        .address = 0x{sym['address']}")
        print("    },")
    
    print("};")
    print("usize __ksyms_count = sizeof(__ksyms) / sizeof(KernelSymbol);")
