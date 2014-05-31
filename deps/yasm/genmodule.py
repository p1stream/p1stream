#!/usr/bin/env python

import sys

infile = sys.argv[1]
outfile = sys.argv[2]

out = open(outfile, 'w')
for line in open(infile, 'r'):
    if line == 'EXTERN_LIST\n':
        out.write("""
            extern yasm_arch_module yasm_x86_LTX_arch;
            extern yasm_listfmt_module yasm_nasm_LTX_listfmt;
            extern yasm_parser_module yasm_gas_LTX_parser;
            extern yasm_parser_module yasm_gnu_LTX_parser;
            extern yasm_parser_module yasm_nasm_LTX_parser;
            extern yasm_parser_module yasm_tasm_LTX_parser;
            extern yasm_preproc_module yasm_nasm_LTX_preproc;
            extern yasm_preproc_module yasm_tasm_LTX_preproc;
            extern yasm_preproc_module yasm_raw_LTX_preproc;
            extern yasm_preproc_module yasm_cpp_LTX_preproc;
            extern yasm_preproc_module yasm_gas_LTX_preproc;
            extern yasm_dbgfmt_module yasm_dwarf2_LTX_dbgfmt;
            extern yasm_dbgfmt_module yasm_null_LTX_dbgfmt;
            extern yasm_objfmt_module yasm_macho_LTX_objfmt;
            extern yasm_objfmt_module yasm_macho32_LTX_objfmt;
            extern yasm_objfmt_module yasm_macho64_LTX_objfmt;
        """)
    elif line == 'MODULES_arch_\n':
        out.write("""
            {"x86", &yasm_x86_LTX_arch}
        """)
    elif line == 'MODULES_dbgfmt_\n':
        out.write("""
            {"dwarf2", &yasm_dwarf2_LTX_dbgfmt},
            {"null", &yasm_null_LTX_dbgfmt}
        """)
    elif line == 'MODULES_objfmt_\n':
        out.write("""
            {"macho", &yasm_macho_LTX_objfmt},
            {"macho32", &yasm_macho32_LTX_objfmt},
            {"macho64", &yasm_macho64_LTX_objfmt}
        """)
    elif line == 'MODULES_listfmt_\n':
        out.write("""
            {"nasm", &yasm_nasm_LTX_listfmt}
        """)
    elif line == 'MODULES_parser_\n':
        out.write("""
            {"gas", &yasm_gas_LTX_parser},
            {"gnu", &yasm_gnu_LTX_parser},
            {"nasm", &yasm_nasm_LTX_parser},
            {"tasm", &yasm_tasm_LTX_parser}
        """)
    elif line == 'MODULES_preproc_\n':
        out.write("""
            {"nasm", &yasm_nasm_LTX_preproc},
            {"tasm", &yasm_tasm_LTX_preproc},
            {"raw", &yasm_raw_LTX_preproc},
            {"cpp", &yasm_cpp_LTX_preproc},
            {"gas", &yasm_gas_LTX_preproc}
        """)
    else:
        out.write(line)
