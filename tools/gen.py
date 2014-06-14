#!/usr/bin/env python

import os
import os.path
import sys
from glob import glob
from fnmatch import fnmatch
from ninja_syntax import Writer
n = Writer(sys.stdout)

if len(sys.argv) == 2 and sys.argv[1] == '--debug':
    cfg_cflags = '-g'
else:
    cfg_cflags = '-O3'

n.variable('builddir', 'out')
n.rule('cp', 'cp $in $out')
n.rule('link', 'clang++ $ldflags -mmacosx-version-min=10.8 -fcolor-diagnostics'
                      ' -stdlib=libc++ -arch x86_64 -o $out $in')
clang = 'clang -mmacosx-version-min=10.8 -stdlib=libc++ -arch x86_64 %s ' \
        '-DNDEBUG -D__POSIX__ -D_GNU_SOURCE -D_LARGEFILE_SOURCE ' \
        '-D_DARWIN_USE_64_BIT_INODE=1 -D_FILE_OFFSET_BITS=64 ' \
        '-fno-exceptions -fno-rtti -fno-threadsafe-statics ' \
        '-fno-strict-aliasing -fcolor-diagnostics' % (cfg_cflags)

def removeMany(paths, remove):
    for x in remove:
        if x in paths:
            paths.remove(x)

def indir(name, paths):
    return ['%s/%s' % (name, path) for path in paths]

def setext(ext, paths):
    return ['%s%s' % (path.rsplit('.', 1)[0], ext) for path in paths]

def outof(paths, ext='.o', outdir='out', flatten=False):
    res = []
    for path in paths:
        if flatten:
            path = path.rsplit('/', 1)[1]
        if not path.startswith(outdir + '/'):
            path = '%s/%s' % (outdir, path)
        res.append(path)
    if ext:
        res = setext(ext, res)
    return res

def copytree(srcdir, outdir, pattern):
    for (base, dirs, files) in os.walk(srcdir):
        if 'native' in base:
            continue
        reldir = os.path.relpath(base, srcdir)
        for f in files:
            if not fnmatch(f, pattern):
                continue
            i = os.path.join(base, f)
            o = os.path.join(outdir, reldir, f)
            n.build(o, 'cp', i)


# FraunhoferAAC
aac_cflags = ' '.join(['-I %s' % d for d in glob('deps/aac/aac/*/include')])
n.rule('aac_cc', '%s -std=c++98 -w %s -c -MMD -MF $out.d -o $out $in' % (clang, aac_cflags),
        deps='gcc', depfile='$out.d')

aac_in = glob('deps/aac/aac/*/src/*.cpp')
aac_out = outof(aac_in)
for (i, o) in zip(aac_in, aac_out):
    n.build(o, 'aac_cc', i)


# YASM
n.rule('yasm_cc', '%s -std=c89 -w -c -DHAVE_CONFIG_H -MMD -MF $out.d'
                  ' -I deps/yasm/generated -I deps/yasm/yasm'
                  ' -I out/deps/yasm -o $out $in' % clang,
        deps='gcc', depfile='$out.d')

yasm_genmodule = 'deps/yasm/genmodule.py'
n.rule('yasm_genmodule', '%s $in $out' % yasm_genmodule)

def yasm_build_tool(name, sources, extra=None):
    binary = 'out/deps/yasm/%s' % name
    objs = outof(sources, outdir='out/deps/yasm', flatten=True)
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
yasm_out_module = outof(yasm_in_module, ext='.c', outdir='out/deps/yasm', flatten=True)
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
yasm_out_re = outof(yasm_in_re, ext='.c', outdir='out/deps/yasm', flatten=True)
for (i, o) in zip(yasm_in_re, yasm_out_re):
    n.build(o, 'yasm_re2c', i, implicit=yasm_re2c)

yasm_in_gperf = indir('deps/yasm/yasm/modules/arch/x86', [
    'x86cpu.gperf',
    'x86regtmod.gperf'
]) + indir('out/deps/yasm', [
    'x86insn_nasm.gperf',
    'x86insn_gas.gperf'
])
yasm_out_gperf = outof(yasm_in_gperf, ext='.c', outdir='out/deps/yasm', flatten=True)
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
yasm_out = outof(yasm_in, outdir='out/deps/yasm', flatten=True)
for (i, o) in zip(yasm_in, yasm_out):
    n.build(o, 'yasm_cc', i)

yasm_in_x86id = [
    'deps/yasm/yasm/modules/arch/x86/x86id.c'
]
yasm_out_x86id = outof(yasm_in_x86id, outdir='out/deps/yasm', flatten=True)
yasm_implicit_x86id = indir('out/deps/yasm', [
    'x86insn_nasm.c',
    'x86insn_gas.c',
    'x86insns.c'
])
for (i, o) in zip(yasm_in_x86id, yasm_out_x86id):
    n.build(o, 'yasm_cc', i, implicit=yasm_implicit_x86id)

yasm = 'out/yasm'
n.build(yasm, 'link', yasm_out + yasm_out_x86id)


# x264
x264_cflags = '-I deps/x264/generated -I out/deps/x264/x264 -I deps/x264/x264'
n.rule('x264_cc', '%s -std=c99 -w %s -c -MMD -MF $out.d -o $out $in' % \
        (clang, x264_cflags), deps='gcc', depfile='$out.d')
n.rule('x264_yasm', '%s -f macho64 -m amd64 -DPIC -DPREFIX'
                      ' -DHAVE_ALIGNED_STACK=1 -DPIC -DHIGH_BIT_DEPTH=0'
                      ' -DBIT_DEPTH=8 -DARCH_X86_64=1 -Ix264/common/x86'
                      ' -o $out $in' % yasm)

x264_cltostr = 'deps/x264/x264/tools/cltostr.pl'
n.rule('x264_cltostr', 'cat $in | perl %s $var > $out' % x264_cltostr)

x264_in_cl = glob('deps/x264/x264/common/opencl/*.cl')
x264_out_cl = ['out/deps/x264/x264/common/oclobj.h']
n.build(x264_out_cl, 'x264_cltostr', x264_in_cl,
        implicit=x264_cltostr, variables={ 'var': 'x264_opencl_source' })

x264_in_c = indir('deps/x264/x264', [
    'common/mc.c',
    'common/predict.c',
    'common/pixel.c',
    'common/macroblock.c',
    'common/frame.c',
    'common/dct.c',
    'common/cpu.c',
    'common/cabac.c',
    'common/common.c',
    'common/osdep.c',
    'common/rectangle.c',
    'common/set.c',
    'common/quant.c',
    'common/deblock.c',
    'common/vlc.c',
    'common/mvpred.c',
    'common/bitstream.c',
    'common/threadpool.c',
    'common/x86/mc-c.c',
    'common/x86/predict-c.c',
    'encoder/analyse.c',
    'encoder/me.c',
    'encoder/ratecontrol.c',
    'encoder/set.c',
    'encoder/macroblock.c',
    'encoder/cabac.c',
    'encoder/cavlc.c',
    'encoder/encoder.c',
    'encoder/lookahead.c',
    'encoder/slicetype-cl.c'
])
x264_out_c = outof(x264_in_c)
for (i, o) in zip(x264_in_c, x264_out_c):
    n.build(o, 'x264_cc', i)

x264_in_opencl = [
    'deps/x264/x264/common/opencl.c'
]
x264_out_opencl = outof(x264_in_opencl)
for (i, o) in zip(x264_in_opencl, x264_out_opencl):
    n.build(o, 'x264_cc', i, implicit=x264_out_cl)

x264_in_asm = indir('deps/x264/x264/common/x86', [
    'bitstream-a.asm',
    'cabac-a.asm',
    'const-a.asm',
    'cpu-a.asm',
    'dct-64.asm',
    'dct-a.asm',
    'deblock-a.asm',
    'mc-a.asm',
    'mc-a2.asm',
    'pixel-a.asm',
    'predict-a.asm',
    'quant-a.asm',
    'sad-a.asm',
    'trellis-64.asm',
    'x86inc.asm',
    'x86util.asm',
])
x264_out_asm = outof(x264_in_asm)
for (i, o) in zip(x264_in_asm, x264_out_asm):
    n.build(o, 'x264_yasm', i, implicit=yasm)

x264_out = x264_out_c + x264_out_opencl + x264_out_asm


# zlib
zlib_cflags = '-I deps/node/node/deps/zlib'
n.rule('zlib_cc', '%s -std=c89 -w %s '
                  '-I out/deps/node/node/deps/zlib/contrib/minizip '
                  '-c -MMD -MF $out.d -o $out $in' % (clang, zlib_cflags),
        deps='gcc', depfile='$out.d')

zlib_in = glob('deps/node/node/deps/zlib/*.c') +\
          indir('deps/node/node/deps/zlib/contrib/minizip', [
    'ioapi.c',
    'unzip.c',
    'zip.c',
])
zlib_out = outof(zlib_in)
for (i, o) in zip(zlib_in, zlib_out):
    n.build(o, 'zlib_cc', i)


# c-ares
cares_cflags = '-I deps/node/node/deps/cares/include'
n.rule('cares_cc', '%s -std=gnu89 -w %s -DCARES_STATICLIB -DHAVE_CONFIG_H '
                   '-I deps/node/node/deps/cares/config/darwin '
                   '-c -MMD -MF $out.d -o $out $in' % (clang, cares_cflags),
        deps='gcc', depfile='$out.d')

cares_in = glob('deps/node/node/deps/cares/src/*.c')
removeMany(cares_in, indir('deps/node/node/deps/cares/src', [
    'windows_port.c',
    'ares_getenv.c',
    'ares_platform.c'
]))
cares_out = outof(cares_in)
for (i, o) in zip(cares_in, cares_out):
    n.build(o, 'cares_cc', i)


# http-parser
http_cflags = '-I deps/node/node/deps/http_parser'
n.rule('http_cc', '%s -std=c89 -w %s -c -MMD -MF $out.d -o $out $in' % \
        (clang, http_cflags), deps='gcc', depfile='$out.d')

http_in = ['deps/node/node/deps/http_parser/http_parser.c']
http_out = outof(http_in)
for (i, o) in zip(http_in, http_out):
    n.build(o, 'http_cc', i)


# libuv
uv_cflags = '-I deps/node/node/deps/uv/include'
n.rule('uv_cc', '%s -std=c89 -w %s ' \
                '-I deps/node/node/deps/uv/include/uv-private '
                '-I deps/node/node/deps/uv/src '
                '-c -MMD -MF $out.d -o $out $in' % (clang, uv_cflags),
       deps='gcc', depfile='$out.d')

uv_in = indir('deps/node/node/deps/uv/src', [
    'fs-poll.c',
    'inet.c',
    'uv-common.c',
    'version.c',
    'unix/async.c',
    'unix/core.c',
    'unix/dl.c',
    'unix/error.c',
    'unix/fs.c',
    'unix/getaddrinfo.c',
    'unix/loop.c',
    'unix/loop-watcher.c',
    'unix/pipe.c',
    'unix/poll.c',
    'unix/process.c',
    'unix/signal.c',
    'unix/stream.c',
    'unix/tcp.c',
    'unix/thread.c',
    'unix/threadpool.c',
    'unix/timer.c',
    'unix/tty.c',
    'unix/udp.c',
    'unix/proctitle.c',
    'unix/darwin.c',
    'unix/fsevents.c',
    'unix/darwin-proctitle.c',
    'unix/kqueue.c'
])
uv_out = outof(uv_in)
for (i, o) in zip(uv_in, uv_out):
    n.build(o, 'uv_cc', i)


# V8
v8_cflags = '-I deps/node/node/deps/v8/include'
n.rule('v8_cc', '%s -std=c++11 -w -I deps/node/node/deps/v8/src %s '
                '-DENABLE_DEBUGGER_SUPPORT -DENABLE_EXTRA_CHECKS '
                '-DV8_TARGET_ARCH_X64 -fstrict-aliasing '
                '-c -MMD -MF $out.d -o $out $in' % (clang, v8_cflags),
        deps='gcc', depfile='$out.d')

v8_js2c = 'deps/node/node/deps/v8/tools/js2c.py'
n.rule('v8_js2c', 'python %s $out $type off $in' % v8_js2c)

v8_postmortem = 'deps/node/node/deps/v8/tools/gen-postmortem-metadata.py'
n.rule('v8_postmortem', 'python %s $out $in' % v8_postmortem)

v8_in_lib = indir('deps/node/node/deps/v8/src', [
    'runtime.js',
    'v8natives.js',
    'array.js',
    'string.js',
    'uri.js',
    'math.js',
    'messages.js',
    'apinatives.js',
    'debug-debugger.js',
    'mirror-debugger.js',
    'liveedit-debugger.js',
    'date.js',
    'json.js',
    'regexp.js',
    'macros.py'
])
v8_out_lib = ['out/deps/v8/libraries.cc']
n.build(v8_out_lib, 'v8_js2c', v8_in_lib,
        variables={ 'type': 'CORE' },
        implicit=v8_js2c)

v8_in_libx = indir('deps/node/node/deps/v8/src', [
    'macros.py',
    'proxy.js',
    'collection.js'
])
v8_out_libx = ['out/deps/v8/experimental-libraries.cc']
n.build(v8_out_libx, 'v8_js2c', v8_in_libx,
        variables={ 'type': 'EXPERIMENTAL' },
        implicit=v8_js2c)

v8_in_postmortem = indir('deps/node/node/deps/v8/src', [
    'objects.h',
    'objects-inl.h'
])
v8_out_postmortem = 'out/deps/v8/debug-support.cc'
n.build(v8_out_postmortem, 'v8_postmortem', v8_in_postmortem,
        implicit=v8_postmortem)

v8_in_base = indir('deps/node/node/deps/v8/src', [
    'accessors.cc',
    'allocation.cc',
    'api.cc',
    'assembler.cc',
    'ast.cc',
    'atomicops_internals_x86_gcc.cc',
    'bignum-dtoa.cc',
    'bignum.cc',
    'bootstrapper.cc',
    'builtins.cc',
    'cached-powers.cc',
    'checks.cc',
    'circular-queue.cc',
    'code-stubs.cc',
    'codegen.cc',
    'compilation-cache.cc',
    'compiler.cc',
    'contexts.cc',
    'conversions.cc',
    'counters.cc',
    'cpu-profiler.cc',
    'data-flow.cc',
    'date.cc',
    'dateparser.cc',
    'debug-agent.cc',
    'debug.cc',
    'deoptimizer.cc',
    'disassembler.cc',
    'diy-fp.cc',
    'dtoa.cc',
    'elements-kind.cc',
    'elements.cc',
    'execution.cc',
    'extensions/externalize-string-extension.cc',
    'extensions/gc-extension.cc',
    'extensions/statistics-extension.cc',
    'factory.cc',
    'fast-dtoa.cc',
    'fixed-dtoa.cc',
    'flags.cc',
    'frames.cc',
    'full-codegen.cc',
    'func-name-inferrer.cc',
    'gdb-jit.cc',
    'global-handles.cc',
    'handles.cc',
    'heap-profiler.cc',
    'heap.cc',
    'hydrogen-instructions.cc',
    'hydrogen.cc',
    'ic.cc',
    'incremental-marking.cc',
    'inspector.cc',
    'interface.cc',
    'interpreter-irregexp.cc',
    'isolate.cc',
    'jsregexp.cc',
    'lithium-allocator.cc',
    'lithium.cc',
    'liveedit.cc',
    'liveobjectlist.cc',
    'log-utils.cc',
    'log.cc',
    'mark-compact.cc',
    'messages.cc',
    'objects-debug.cc',
    'objects-printer.cc',
    'objects-visiting.cc',
    'objects.cc',
    'once.cc',
    'optimizing-compiler-thread.cc',
    'parser.cc',
    'preparse-data.cc',
    'preparser.cc',
    'prettyprinter.cc',
    'profile-generator.cc',
    'property.cc',
    'regexp-macro-assembler-irregexp.cc',
    'regexp-macro-assembler-tracer.cc',
    'regexp-macro-assembler.cc',
    'regexp-stack.cc',
    'rewriter.cc',
    'runtime-profiler.cc',
    'runtime.cc',
    'safepoint-table.cc',
    'scanner-character-streams.cc',
    'scanner.cc',
    'scopeinfo.cc',
    'scopes.cc',
    'serialize.cc',
    'snapshot-common.cc',
    'spaces.cc',
    'store-buffer.cc',
    'string-search.cc',
    'string-stream.cc',
    'strtod.cc',
    'stub-cache.cc',
    'token.cc',
    'transitions.cc',
    'type-info.cc',
    'unicode.cc',
    'utils.cc',
    'v8-counters.cc',
    'v8.cc',
    'v8conversions.cc',
    'v8threads.cc',
    'v8utils.cc',
    'variables.cc',
    'version.cc',
    'zone.cc',
    'x64/assembler-x64.cc',
    'x64/builtins-x64.cc',
    'x64/code-stubs-x64.cc',
    'x64/codegen-x64.cc',
    'x64/cpu-x64.cc',
    'x64/debug-x64.cc',
    'x64/deoptimizer-x64.cc',
    'x64/disasm-x64.cc',
    'x64/frames-x64.cc',
    'x64/full-codegen-x64.cc',
    'x64/ic-x64.cc',
    'x64/lithium-codegen-x64.cc',
    'x64/lithium-gap-resolver-x64.cc',
    'x64/lithium-x64.cc',
    'x64/macro-assembler-x64.cc',
    'x64/regexp-macro-assembler-x64.cc',
    'x64/stub-cache-x64.cc',
    'platform-macos.cc',
    'platform-posix.cc'
]) + indir('out/deps/v8', [
    'libraries.cc',
    'experimental-libraries.cc',
    'debug-support.cc'
])
v8_out_base = outof(v8_in_base)
for (i, o) in zip(v8_in_base, v8_out_base):
    n.build(o, 'v8_cc', i)

v8_mksnapshot = 'out/deps/v8/mksnapshot'
v8_mksnapshot_in = indir('deps/node/node/deps/v8/src', [
    'snapshot-empty.cc',
    'mksnapshot.cc'
])
v8_mksnapshot_out = outof(v8_mksnapshot_in)
for (i, o) in zip(v8_mksnapshot_in, v8_mksnapshot_out):
    n.build(o, 'v8_cc', i)
n.build(v8_mksnapshot, 'link', v8_out_base + v8_mksnapshot_out)
n.rule('v8_mksnapshot', '%s --log-snapshot-positions'
                          ' --logfile $out' % v8_mksnapshot)

v8_snapshot_out = [
    'out/snapshot.log',
    'out/deps/v8/snapshot.cc'
]
n.build(v8_snapshot_out, 'v8_mksnapshot', implicit=v8_mksnapshot)

v8_in = [
    'out/deps/v8/snapshot.cc'
]
v8_out = outof(v8_in)
for (i, o) in zip(v8_in, v8_out):
    n.build(o, 'v8_cc', i)
v8_out += v8_out_base


# node.js
node_cflags = '-I deps/node/node/src %s %s' % (uv_cflags, v8_cflags)
n.rule('node_cc', '%s -std=c++11 -w %s -I out/deps/node %s %s %s '
                  '-DNODE_WANT_INTERNALS=1 -DARCH=\\"x64\\" '
                  '-DNODE_TAG=\\"\\" -DPLATFORM=\\"darwin\\" '
                  '-c -MMD -MF $out.d -o $out $in' %
                  (clang, node_cflags, cares_cflags, http_cflags, zlib_cflags),
        deps='gcc', depfile='$out.d')

node_js2c = 'deps/node/node/tools/js2c.py'
n.rule('node_js2c', '%s $out $in' % node_js2c)

node_in_lib = glob('deps/node/node/lib/*.js') + \
              glob('deps/node/node/src/*.js') + \
              ['deps/node/config.gypi'] + \
              ['core/native/mac/_third_party_main.js']
node_out_lib = ['out/deps/node/node_natives.h']
n.build(node_out_lib, 'node_js2c', node_in_lib, implicit=node_js2c)

node_in_js = [
    'deps/node/node/src/node_javascript.cc'
]
node_out_js = outof(node_in_js)
n.build(node_out_js, 'node_cc', node_in_js, implicit=node_out_lib)

node_in = indir('deps/node/node/src', [
    'fs_event_wrap.cc',
    'cares_wrap.cc',
    'handle_wrap.cc',
    'node.cc',
    'node_buffer.cc',
    'node_constants.cc',
    'node_extensions.cc',
    'node_file.cc',
    'node_http_parser.cc',
    'node_main.cc',
    'node_os.cc',
    'node_script.cc',
    'node_stat_watcher.cc',
    'node_string.cc',
    'node_zlib.cc',
    'pipe_wrap.cc',
    'signal_wrap.cc',
    'string_bytes.cc',
    'stream_wrap.cc',
    'slab_allocator.cc',
    'tcp_wrap.cc',
    'timer_wrap.cc',
    'tty_wrap.cc',
    'process_wrap.cc',
    'v8_typed_array.cc',
    'udp_wrap.cc'
])
node_out = outof(node_in)
for (i, o) in zip(node_in, node_out):
    n.build(o, 'node_cc', i)

node = 'out/P1stream.app/Contents/MacOS/P1stream'
n.build(node, 'link',
        node_out + node_out_js + zlib_out +
        cares_out + http_out + uv_out + v8_out,
        variables={
            'ldflags': '-framework CoreFoundation -framework Carbon'
        })


# P1stream
mod_dir = 'out/P1stream.app/Contents/Modules'
n.rule('mod_cc', '%s -std=c++11 -Wall -Werror -DBUILDING_NODE_EXTENSION '
                 '%s $cflags -c -MMD -MF $out.d -o $out $in' %
                 (clang, node_cflags),
        deps='gcc', depfile='$out.d')

def build_mod(name, res=['*.js'], src=None, extra_obj=None, cflags='', ldflags=''):
    mod = '%s/%s/%s.node' % (mod_dir, name, name)

    if not src:
        src = glob('%s/native/*.cc' % name)
    obj = outof(src)
    for (i, o) in zip(src, obj):
        n.build(o, 'mod_cc', i, variables={ 'cflags': cflags })
    if extra_obj:
        obj += extra_obj
    if len(obj) > 0:
        ldflags = ('-dynamiclib -install_name @rpath/%s'
                  ' -undefined dynamic_lookup %s' % (mod, ldflags))
        n.build(mod, 'link', obj, variables={ 'ldflags': ldflags })

    for pattern in res:
        copytree(name, '%s/%s' % (mod_dir, name), pattern)

build_mod('core',
    src =
        glob('core/native/*.cc') +
        glob('core/native/mac/*.mm'),
    extra_obj =
        aac_out + x264_out,
    cflags =
        '-I core/native '
        '-I core/native/mac ',
    ldflags =
        '-framework Cocoa '
        '-framework IOSurface '
        '-framework OpenGL '
        '-framework OpenCL')

build_mod('mac_sources',
    cflags =
        '-I core/native '
        '-I mac-sources/native',
    ldflags =
        '-framework CoreVideo')

n.build('out/P1stream.app/Contents/Resources/Info.plist',
        'cp', 'core/native/mac/Info.plist')
