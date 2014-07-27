{
    'target_defaults': {
        'conditions': [
            ['OS != "win"', {
                'cflags': ['-std=c++11'],
                'ldflags': ['-Wl,-Bsymbolic']
            }]
        ]
    },
    'targets': [
        {
            'target_name': 'api',
            'sources': [ 'api/native/module.cc' ]
        },
        {
            'target_name': 'core',
            'dependencies': [
                'deps/aac/aac.gyp:libFDK',
                'deps/aac/aac.gyp:libSYS',
                'deps/aac/aac.gyp:libMpegTPEnc',
                'deps/aac/aac.gyp:libSBRenc',
                'deps/aac/aac.gyp:libAACenc',
                'deps/x264/x264.gyp:libx264common',
                'deps/x264/x264.gyp:libx264encoder'
            ],
            'sources': [
                'core/native/audio.cc',
                'core/native/module.cc',
                'core/native/util.cc',
                'core/native/video.cc'
            ],
            'conditions': [
                ['OS == "mac"', {
                    'sources': [
                        'core/native/util_mac.cc',
                        'core/native/video_mac.mm'
                    ]
                }],
                ['OS == "linux"', {
                    'sources': [
                        'core/native/util_linux.cc',
                        'core/native/video_linux.cc'
                    ]
                }]
            ]
        }
    ]
}
