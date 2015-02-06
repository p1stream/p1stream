#!/usr/bin/env iojs

var fs = require('fs');
var path = require('path');
var bu = require('./tools/build_util');

// Ensure correct directory.
process.chdir(path.dirname(process.mainModule.filename));
// Add our own npm bin dir to path.
process.env.PATH = [
    path.join(process.cwd(), 'tools', 'node_modules', '.bin'),
    process.env.PATH
].join(':');

bu.taskRunner({

    //////////
    // The following are the individual build steps that make up a complete
    // build, ie. `./build.js all`

    // Install certain build tools locally in `tools/node_modules/`, so that
    // these don't have to be setup globally.
    'install-tools': function(cb) {
        var deps = require('./package.json').devToolDependencies;
        var tasks = Object.keys(deps).map(function(name) {
            var arg = name + '@' + deps[name];
            return [bu.run, 'npm --prefix=tools install ' + arg];
        });
        bu.chain(tasks, cb);
    },

    // Update submodules.
    'sync-submodules': function(cb) {
        bu.chain([
            [bu.run, 'git submodule init'],
            [bu.run, 'git submodule sync'],
            [bu.run, 'git submodule update']
        ], cb);
    },

    // Install all our regular npm dependencies. This expects `p1-build` to be
    // setup using the `install-tools` task.
    'npm-install': function(cb) {
        bu.run('p1-build npm install', cb);
    },

    // Install all our bower dependencies for the web ui.
    'bower-install': function(cb) {
        bu.run('bower install', cb);
    },

    // Install our plugin dependencies. These are the ones we bundle.
    'install-plugins': function(cb) {
        var deps = require('./package.json').pluginDependencies;
        deps = bu.extend({}, deps.all, deps[process.platform]);
        var tasks = Object.keys(deps).map(function(name) {
            var arg = name + '@' + deps[name];
            return [bu.run, 'p1-build npm install ' + arg];
        });
        bu.chain(tasks, cb);
    },

    // The meta task to do a complete build.
    'all': [
        'install-tools',
        'sync-submodules',
        'npm-install',
        'bower-install',
        'install-plugins'
    ],

    //////////
    // The following are tasks that are useful in the addition to the above
    // individual build steps.

    'npm-update': function(cb) {
        bu.run('p1-build npm update', cb);
    },

    'bower-update': function(cb) {
        bu.run('bower update', cb);
    },

    // Incremental build of just the core native module.
    'native': function(cb) {
        bu.run('p1-build node-gyp build', cb);
    }

});
