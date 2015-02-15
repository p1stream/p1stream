{
    'targets': [
        {
            'target_name': 'native',
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
                '$(p1stream_include_dir)'
            ],
            'sources': [
                'src/audio.cc',
                'src/module.cc',
                'src/software_clock.cc',
                'src/util.cc',
                'src/video.cc'
            ],
            'conditions': [
                ['OS == "mac"', {
                    'xcode_settings': {
                        # 'MACOSX_DEPLOYMENT_TARGET': '10.8',  # FIXME: This doesn't work
                        'OTHER_CFLAGS': ['-mmacosx-version-min=10.8', '-std=c++11', '-stdlib=libc++']
                    },
                    'link_settings': {
                        'libraries': [
                            '$(SDKROOT)/System/Library/Frameworks/IOSurface.framework',
                        ]
                    },
                    'sources': [
                        'src/util_mac.cc',
                        'src/video_mac.mm'
                    ]
                }],
                ['OS == "linux"', {
                    'cflags': ['-std=c++11'],
                    'ldflags': ['-Wl,-Bsymbolic'],
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
