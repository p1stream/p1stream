#!/usr/bin/env node

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
    // build, ie. `node build.js all`

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
    },

    // Download the atom-shell tarball.
    'download-atom-shell': function(cb) {
        var request = require('request');

        var params = require('./tools/node_modules/p1-build');
        var file = path.join('atom-shell', params.atomShellPackage);

        if (fs.existsSync(file)) return cb();

        bu.logStep('downloading ' + file);

        try { fs.mkdirSync('atom-shell'); }
        catch (err) { if (err.code !== 'EEXIST') return cb(err); }

        var counter = 0;
        var stream = fs.createWriteStream(file)
            .on('error', cb)
            .on('finish', cb);
        request(params.atomShellPackageUrl)
            .on('error', cb)
            .on('data', function(chunk) {
                counter += chunk.length;
                while (counter > 102400) {
                    counter %= 102400;
                    process.stdout.write('.');
                }
            })
            .on('end', function() {
                process.stdout.write('\n');
            })
            .pipe(stream);
    },

    // Extract the atom-shell tarball.
    'extract-atom-shell': function(cb) {
        var params = require('./tools/node_modules/p1-build');
        var dir = path.join('atom-shell', 'v' + params.atomShellVersion);
        var file = path.join('atom-shell', params.atomShellPackage);

        if (fs.existsSync(dir)) return cb();

        bu.run('unzip -q ' + file + ' -d ' + dir, cb);
    },

    // Run atom-shell with the P1stream source directory.
    // Downloads and extracts atom-shell if it's not already present.
    'run': [
        'download-atom-shell',
        'extract-atom-shell',
        function(cb) {
            var params = require('./tools/node_modules/p1-build');
            var dir = path.join('atom-shell', 'v' + params.atomShellVersion);

            var cmd;
            if (process.platform === 'darwin')
                cmd = path.join(dir, 'Atom.app', 'Contents', 'MacOS', 'Atom');
            else if (process.platform === 'linux')
                cmd = path.join(dir, 'atom');
            else if (process.platform === 'win32')
                cmd = path.join(dir, 'atom.exe');
            else
                cb(new Error("Unsupported platform"));

            bu.run(cmd + ' ' + process.cwd(), cb);
        }
    ]

});
