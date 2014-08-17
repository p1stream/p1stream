#!/usr/bin/env node

var path = require('path');
var bu = require('./tools/build_util');

// Ensure correct directory.
process.chdir(path.dirname(process.mainModule.filename));
// Add our own npm bin dir to path.
process.env.PATH = [
    path.join(process.cwd(), 'node_modules', '.bin'),
    process.env.PATH
].join(':');

bu.taskRunner({

    'install-tools': function(cb) {
        bu.run('npm install p1stream/p1-build node-gyp bower', cb);
    },

    'sync-submodules': function(cb) {
        bu.chain([
            [bu.run, 'git submodule init'],
            [bu.run, 'git submodule sync'],
            [bu.run, 'git submodule update']
        ], cb);
    },

    'bootstrap': [
        'install-tools',
        'sync-submodules'
    ],

    'npm-install': function(cb) {
        bu.run('p1-build npm install', cb);
    },

    'npm-update': function(cb) {
        bu.run('p1-build npm update', cb);
    },

    'bower-install': function(cb) {
        bu.run('bower install', cb);
    },

    'bower-update': function(cb) {
        bu.run('bower update', cb);
    },

    'all': [
        'bootstrap',
        'npm-install',
        'bower-install'
    ],

    'native': function(cb) {
        bu.run('p1-build node-gyp build', cb);
    }

});
