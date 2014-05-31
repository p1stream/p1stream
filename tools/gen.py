#!/usr/bin/env python

import sys
from glob import glob
from ninja_syntax import Writer
n = Writer(sys.stdout)

n.variable('builddir', 'out')

# FraunhoferAAC
aac_cflags = ' '.join(['-I %s' % d for d in glob('deps/aac/aac/*/include')])
aac_srcs = glob('deps/aac/aac/*/src/*.cpp')
aac_objs = ['out/%s.o' % s[:-4] for s in aac_srcs]
n.rule('aac_cc', 'clang -std=c++98 -w %s -c -MMD -MF $out.d -o $out $in' % aac_cflags,
        deps='gcc', depfile='$out.d')
for (src, obj) in zip(aac_srcs, aac_objs):
    n.build(obj, 'aac_cc', src)
