#!/usr/bin/env node

var fs = require('fs');
var path = require('path');
var request = require('request');
var bu = require('./tools/build_util');

// Ensure correct directory.
process.chdir(path.dirname(process.mainModule.filename));
// Add our own npm bin dir to path.
process.env.PATH = [
    path.join(process.cwd(), 'tools', 'node_modules', '.bin'),
    process.env.PATH
].join(':');

bu.taskRunner({

    'install-tools': function(cb) {
        bu.run('npm --prefix=tools ' +
            'install p1stream/p1-build node-gyp bower', cb);
    },

    'sync-submodules': function(cb) {
        bu.chain([
            [bu.run, 'git submodule init'],
            [bu.run, 'git submodule sync'],
            [bu.run, 'git submodule update']
        ], cb);
    },

    'download-atom-shell': function(cb) {
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

    'extract-atom-shell': function(cb) {
        var params = require('./tools/node_modules/p1-build');
        var dir = path.join('atom-shell', 'v' + params.atomShellVersion);
        var file = path.join('atom-shell', params.atomShellPackage);

        if (fs.existsSync(dir)) return cb();

        bu.run('unzip -q ' + file + ' -d ' + dir, cb);
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
    },

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
