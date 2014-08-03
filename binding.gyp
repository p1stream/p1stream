{
    'targets': [
        {
            'target_name': 'p1stream',
            'dependencies': [
                'deps/aac/aac.gyp:libFDK',
                'deps/aac/aac.gyp:libSYS',
                'deps/aac/aac.gyp:libMpegTPEnc',
                'deps/aac/aac.gyp:libSBRenc',
                'deps/aac/aac.gyp:libAACenc',
                'deps/x264/x264.gyp:libx264common',
                'deps/x264/x264.gyp:libx264encoder'
            ],
            'include_dirs': [
                'include'
            ],
            'sources': [
                'src/audio.cc',
                'src/ebml.cc',
                'src/module.cc',
                'src/software_clock.cc',
                'src/util.cc',
                'src/video.cc'
            ],
            'conditions': [
                ['OS != "win"', {
                    'cflags': ['-std=c++11'],
                    'ldflags': ['-Wl,-Bsymbolic']
                }],
                ['OS == "mac"', {
                    'sources': [
                        'src/util_mac.cc',
                        'src/video_mac.mm'
                    ]
                }],
                ['OS == "linux"', {
                    'sources': [
                        'src/util_linux.cc',
                        'src/video_linux.cc'
                    ],
                    'libraries': [
                        '-lEGL', '-lOpenCL'
                    ]
                }]
            ]
        }
    ]
}
