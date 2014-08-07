{
    'variables': {
        'x264_include_dirs': [
            'generated/<(OS)',
            'generated',
            'x264'
        ],
        'yasm_defines': [
            '-DPIC',
            '-DHIGH_BIT_DEPTH=0',
            '-DBIT_DEPTH=8',
            '-DARCH_X86_64=1'
        ]
    },
    'conditions': [
        ['OS == "mac"', {
            'variables': {
                'yasm_format': 'macho64',
                'yasm_defines': [
                    '-DPREFIX',
                    '-DHAVE_ALIGNED_STACK=1',
                ]
            }
        }],
        ['OS == "linux"', {
            'variables': {
                'yasm_format': 'elf',
                'yasm_defines': [
                    '-DSTACK_ALIGNMENT=32'
                ]
            }
        }]
    ],
    'target_defaults': {
        'include_dirs': ['<@(x264_include_dirs)'],
        'all_dependent_settings': {
            'include_dirs': ['<@(x264_include_dirs)']
        },
        'conditions': [
            ['OS == "mac"', {
                'xcode_settings': {
                    'OTHER_CFLAGS': ['-std=gnu99', '-w', '-ffast-math', '-fno-tree-vectorize'],
                    'OTHER_CFLAGS!': [
                        '-Wextra', '-Wall', '-Wno-unused-parameter',
                        '-fno-omit-frame-pointer',
                        '-ffunction-sections',
                        '-fdata-sections',
                        '-fno-tree-vrp',
                        '-fno-tree-sink'
                    ]
                }
            }],
            ['OS == "linux"', {
                'cflags': ['-std=gnu99', '-w', '-ffast-math', '-mpreferred-stack-boundary=5', '-fno-tree-vectorize'],
                'cflags!': [
                    '-Wextra', '-Wall', '-Wno-unused-parameter',
                    '-fno-omit-frame-pointer',
                    '-ffunction-sections',
                    '-fdata-sections',
                    '-fno-tree-vrp',
                    '-fno-tree-sink'
                ]
            }]
        ],
        'rules': [
            {
                'rule_name': 'yasm',
                'extension': 'asm',
                'outputs': ['<(INTERMEDIATE_DIR)/<(RULE_INPUT_ROOT).o'],
                'process_outputs_as_sources': 1,
                'action': ['yasm', '-f', '<(yasm_format)', '-m', 'amd64',
                            '<@(yasm_defines)', '-I', 'x264/common/x86',
                           '-o', '<@(_outputs)', '<(RULE_INPUT_PATH)']
            }
        ]
    },
    'targets': [
        {
            'target_name': 'libx264common',
            'type': 'static_library',
            'sources': [
                'x264/common/mc.c',
                'x264/common/predict.c',
                'x264/common/pixel.c',
                'x264/common/macroblock.c',
                'x264/common/frame.c',
                'x264/common/dct.c',
                'x264/common/cpu.c',
                'x264/common/cabac.c',
                'x264/common/common.c',
                'x264/common/osdep.c',
                'x264/common/rectangle.c',
                'x264/common/set.c',
                'x264/common/quant.c',
                'x264/common/deblock.c',
                'x264/common/vlc.c',
                'x264/common/mvpred.c',
                'x264/common/bitstream.c',
                'x264/common/threadpool.c',
                'x264/common/opencl.c',
                'x264/common/x86/mc-c.c',
                'x264/common/x86/predict-c.c',
                'x264/common/x86/bitstream-a.asm',
                'x264/common/x86/cabac-a.asm',
                'x264/common/x86/const-a.asm',
                'x264/common/x86/cpu-a.asm',
                'x264/common/x86/dct-64.asm',
                'x264/common/x86/dct-a.asm',
                'x264/common/x86/deblock-a.asm',
                'x264/common/x86/mc-a.asm',
                'x264/common/x86/mc-a2.asm',
                'x264/common/x86/pixel-a.asm',
                'x264/common/x86/predict-a.asm',
                'x264/common/x86/quant-a.asm',
                'x264/common/x86/sad-a.asm',
                'x264/common/x86/trellis-64.asm',
                'x264/common/x86/x86inc.asm',
                'x264/common/x86/x86util.asm'
            ]
        },
        {
            'target_name': 'libx264encoder',
            'type': 'static_library',
            'sources': [
                'x264/encoder/analyse.c',
                'x264/encoder/me.c',
                'x264/encoder/ratecontrol.c',
                'x264/encoder/set.c',
                'x264/encoder/macroblock.c',
                'x264/encoder/cabac.c',
                'x264/encoder/cavlc.c',
                'x264/encoder/encoder.c',
                'x264/encoder/lookahead.c',
                'x264/encoder/slicetype-cl.c'
            ]
        }
    ]
}
