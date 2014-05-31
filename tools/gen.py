#!/usr/bin/env python

import sys
from glob import glob
from ninja_syntax import Writer
n = Writer(sys.stdout)

n.variable('builddir', 'out')
n.rule('link', 'clang -o $out $in')

def indir(name, paths):
    return ['%s/%s' % (name, path) for path in paths]

def setext(ext, paths):
    return ['%s%s' % (path.rsplit('.', 1)[0], ext) for path in paths]

def outof(paths, ext='.o', outdir=None):
    res = []
    for path in paths:
        if outdir:
            path = '%s/%s' % (outdir, path.rsplit('/', 1)[1])
        elif not path.startswith('out/'):
            path = 'out/%s' % path
        res.append(path)
    return setext(ext, res)


# FraunhoferAAC
aac_cflags = ' '.join(['-I %s' % d for d in glob('deps/aac/aac/*/include')])
n.rule('aac_cc', 'clang -ansi -w %s -c -MMD -MF $out.d -o $out $in' % aac_cflags,
        deps='gcc', depfile='$out.d')

aac_in = glob('deps/aac/aac/*/src/*.cpp')
aac_out = outof(aac_in)
for (i, o) in zip(aac_in, aac_out):
    n.build(o, 'aac_cc', i)


# YASM
n.rule('yasm_cc', 'clang -ansi -w -c -DHAVE_CONFIG_H -MMD -MF $out.d'
                       ' -I deps/yasm/generated'
                       ' -I deps/yasm/yasm'
                       ' -I out/deps/yasm'
                       ' -o $out $in',
        deps='gcc', depfile='$out.d')

yasm_genmodule = 'deps/yasm/genmodule.py'
n.rule('yasm_genmodule', '%s $in $out' % yasm_genmodule)

def yasm_build_tool(name, sources, extra=None):
    binary = 'out/deps/yasm/%s' % name
    objs = outof(sources, outdir='out/deps/yasm')
    for (i, o) in zip(sources, objs):
        n.build(o, 'yasm_cc', i)
    if extra:
        objs += extra
    n.build(binary, 'link', objs)
    return binary

yasm_genstring = yasm_build_tool('genstring', [
    'deps/yasm/yasm/genstring.c'
])
n.rule('yasm_genstring', '%s $string $out $in' % yasm_genstring)

yasm_genmacro = yasm_build_tool('genmacro', [
    'deps/yasm/yasm/tools/genmacro/genmacro.c'
])
n.rule('yasm_genmacro', '%s $out $var $in' % yasm_genmacro)

yasm_genperf = yasm_build_tool('genperf', indir('deps/yasm/yasm/tools/genperf', [
    'genperf.c',
    'perfect.c'
]), indir('out/deps/yasm', [
    'phash.o',
    'xmalloc.o',
    'xstrdup.o'
]))
n.rule('yasm_genperf', '%s $in $out' % yasm_genperf)

yasm_genversion = yasm_build_tool('genversion', [
    'deps/yasm/yasm/modules/preprocs/nasm/genversion.c'
])
n.rule('yasm_genversion', '%s $out $var $in' % yasm_genversion)

yasm_re2c = yasm_build_tool('re2c', indir('deps/yasm/yasm/tools/re2c', [
    'main.c',
    'code.c',
    'dfa.c',
    'parser.c',
    'actions.c',
    'scanner.c',
    'mbo_getopt.c',
    'substr.c',
    'translate.c'
]))
n.rule('yasm_re2c', '%s -b -o $out $in' % yasm_re2c)

yasm_gen_x86_insn = 'deps/yasm/yasm/modules/arch/x86/gen_x86_insn.py'
n.rule('yasm_gen_x86_insn', 'cd out/deps/yasm && ../../../%s' % yasm_gen_x86_insn)
n.build(indir('out/deps/yasm', [
    'x86insn_nasm.gperf',
    'x86insn_gas.gperf',
    'x86insns.c'
]), 'yasm_gen_x86_insn', implicit=yasm_gen_x86_insn)

yasm_in_license = 'deps/yasm/yasm/COPYING'
yasm_out_license = 'out/deps/yasm/license.c'
n.build(yasm_out_license, 'yasm_genstring', yasm_in_license,
        variables={ 'string': 'license_msg' },
        implicit=yasm_genstring)

yasm_in_module = ['deps/yasm/yasm/libyasm/module.in']
yasm_out_module = outof(yasm_in_module, ext='.c', outdir='out/deps/yasm')
for (i, o) in zip(yasm_in_module, yasm_out_module):
    n.build(o, 'yasm_genmodule', i, implicit=yasm_genmodule)

n.build('out/deps/yasm/version.mac', 'yasm_genversion',
        implicit=yasm_genversion)

n.build('out/deps/yasm/nasm-version.c', 'yasm_genmacro',
        'out/deps/yasm/version.mac',
        variables={ 'var': 'nasm_version_mac' },
        implicit=yasm_genmacro)
n.build('out/deps/yasm/nasm-macros.c', 'yasm_genmacro',
        'deps/yasm/yasm/modules/parsers/nasm/nasm-std.mac',
        variables={ 'var': 'nasm_standard_mac' },
        implicit=yasm_genmacro)

yasm_in_re = indir('deps/yasm/yasm/modules/parsers', [
    'nasm/nasm-token.re',
    'gas/gas-token.re'
])
yasm_out_re = outof(yasm_in_re, ext='.c', outdir='out/deps/yasm')
for (i, o) in zip(yasm_in_re, yasm_out_re):
    n.build(o, 'yasm_re2c', i, implicit=yasm_re2c)

yasm_in_gperf = indir('deps/yasm/yasm/modules/arch/x86', [
    'x86cpu.gperf',
    'x86regtmod.gperf'
]) + indir('out/deps/yasm', [
    'x86insn_nasm.gperf',
    'x86insn_gas.gperf'
])
yasm_out_gperf = outof(yasm_in_gperf, ext='.c', outdir='out/deps/yasm')
for (i, o) in zip(yasm_in_gperf, yasm_out_gperf):
    n.build(o, 'yasm_genperf', i, implicit=yasm_genperf)

yasm_in = indir('deps/yasm/yasm', [
    'libyasm/assocdat.c',
    'libyasm/bc-align.c',
    'libyasm/bc-data.c',
    'libyasm/bc-incbin.c',
    'libyasm/bc-org.c',
    'libyasm/bc-reserve.c',
    'libyasm/bitvect.c',
    'libyasm/bytecode.c',
    'libyasm/errwarn.c',
    'libyasm/expr.c',
    'libyasm/file.c',
    'libyasm/floatnum.c',
    'libyasm/hamt.c',
    'libyasm/insn.c',
    'libyasm/intnum.c',
    'libyasm/inttree.c',
    'libyasm/linemap.c',
    'libyasm/md5.c',
    'libyasm/mergesort.c',
    'libyasm/phash.c',
    'libyasm/section.c',
    'libyasm/strcasecmp.c',
    'libyasm/strsep.c',
    'libyasm/symrec.c',
    'libyasm/valparam.c',
    'libyasm/value.c',
    'libyasm/xmalloc.c',
    'libyasm/xstrdup.c',
    'frontends/yasm/yasm-options.c',
    'frontends/yasm/yasm-plugin.c',
    'frontends/yasm/yasm.c',
    'modules/arch/x86/x86arch.c',
    'modules/arch/x86/x86bc.c',
    'modules/arch/x86/x86expr.c',
    'modules/dbgfmts/dwarf2/dwarf2-aranges.c',
    'modules/dbgfmts/dwarf2/dwarf2-dbgfmt.c',
    'modules/dbgfmts/dwarf2/dwarf2-info.c',
    'modules/dbgfmts/dwarf2/dwarf2-line.c',
    'modules/dbgfmts/null/null-dbgfmt.c',
    'modules/listfmts/nasm/nasm-listfmt.c',
    'modules/objfmts/macho/macho-objfmt.c',
    'modules/parsers/nasm/nasm-parse.c',
    'modules/parsers/nasm/nasm-parser.c',
    'modules/parsers/gas/gas-parse-intel.c',
    'modules/parsers/gas/gas-parse.c',
    'modules/parsers/gas/gas-parser.c',
    'modules/preprocs/raw/raw-preproc.c',
    'modules/preprocs/nasm/nasm-eval.c',
    'modules/preprocs/nasm/nasm-pp.c',
    'modules/preprocs/nasm/nasm-preproc.c',
    'modules/preprocs/nasm/nasmlib.c',
    'modules/preprocs/gas/gas-eval.c',
    'modules/preprocs/gas/gas-preproc.c',
    'modules/preprocs/cpp/cpp-preproc.c'
]) + indir('out/deps/yasm', [
    'x86cpu.c',
    'x86regtmod.c',
    'gas-token.c',
    'nasm-token.c'
]) + yasm_out_module
yasm_out = outof(yasm_in, outdir='out/deps/yasm')
for (i, o) in zip(yasm_in, yasm_out):
    n.build(o, 'yasm_cc', i)

yasm_in_x86id = [
    'deps/yasm/yasm/modules/arch/x86/x86id.c'
]
yasm_out_x86id = outof(yasm_in_x86id, outdir='out/deps/yasm')
yasm_implicit_x86id = indir('out/deps/yasm', [
    'x86insn_nasm.c',
    'x86insn_gas.c',
    'x86insns.c'
])
for (i, o) in zip(yasm_in_x86id, yasm_out_x86id):
    n.build(o, 'yasm_cc', i, implicit=yasm_implicit_x86id)

n.build('out/yasm', 'link', yasm_out + yasm_out_x86id)
