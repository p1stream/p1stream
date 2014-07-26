{
    'target_defaults': {
        'conditions': [
            ['OS != "win"', {
                'cflags': ['-std=c++11']
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
            'include_dirs': [
                'deps/aac/aac/libAACenc/include',
                'deps/aac/aac/libSYS/include',
                'deps/x264/generated/<(OS)',
                'deps/x264/x264'
            ],
            'sources': [
                'core/native/module.cc',
                'core/native/util.cc',
                'core/native/util_mac.cc',
                'core/native/util_linux.cc',
                'core/native/audio.cc',
                'core/native/video.cc',
                'core/native/video_mac.mm'
                'core/native/video_linux.cc'
            ]
        }
    ]
}
