// Our basic task runner.
exports.taskRunner = function(commands) {
    var args = process.argv.slice(2);

    var arr = [];
    var ok = args.length !== 0;
    function pushCommand(name) {
        if (!commands.hasOwnProperty(name)) {
            console.error('No such command: ' + name);
            ok = false;
        }
        else {
            var cmd = commands[name];
            if (!Array.isArray(cmd))
                cmd = [cmd];

            cmd.forEach(function(cmd) {
                if (typeof(cmd) === 'string')
                    pushCommand(cmd);
                else
                    arr.push(cmd);
            });
        }

    }
    args.forEach(pushCommand);

    if (!ok) {
        console.log('Available commands: ' +
            Object.keys(commands).sort().join(', '));
        return process.exit(1);
    }

    exports.chain(arr, function(err) {
        if (err)
            throw err;
    });
};

// Bold line to log between other task runner output.
exports.logStep = function(s, cb) {
    console.log('\u001b[1m - ' + s + '\u001b[22m');
    if (cb) cb();
};

// Similar to child_process.exec, but doesn't buffer stdio.
var spawn = require('child_process').spawn;
exports.run = function(cmd, cb) {
    var options = { stdio: 'inherit' };

    var bin, args;
    if (process.platform === 'win32') {
        bin = process.env.comspec || 'cmd.exe';
        args = ['/s', '/c', '"' + cmd + '"'];
        options.windowsVerbatimArguments = true;
    } else {
        bin = '/bin/sh';
        args = ['-c', cmd];
    }

    exports.logStep(cmd);

    var child = spawn(bin, args, options);
    child.on('error', cb);
    child.on('exit', function(code, signal) {
        if (signal)
            cb(new Error("Child exited with signal " + signal));
        else if (code !== 0)
            cb(new Error("Child exited with status " + code));
        else
            cb();
    });
};

// Simple async chain helper.
exports.chain = function(arr, cb) {
    var length = arr.length;
    var idx = 0;
    next();

    function next() {
        if (idx === length)
            return cb();

        var cmd = arr[idx++];
        if (!Array.isArray(cmd))
            cmd = [cmd];

        var ctx, fn, args;
        ctx = cmd[0];
        if (typeof(ctx) === 'function') {
            fn = ctx;
            args = cmd.slice(1);
            ctx = null;
        }
        else {
            fn = cmd[1];
            args = cmd.slice(2);
        }

        args.push(cmdCb);
        fn.apply(ctx, args);
    }

    function cmdCb(err) {
        if (err)
            cb(err);
        else
            next();
    }
};
