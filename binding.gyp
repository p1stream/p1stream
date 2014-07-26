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
        }
    ]
}
