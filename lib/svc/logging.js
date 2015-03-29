var _ = require('lodash');
var path = require('path');
var util = require('util');
var chalk = require('chalk');
var bunyan = require('bunyan');
var userPaths = require('../userPaths');
var native = require('../../build/Release/native.node');

module.exports = function(app) {
    var streams = [];

    // Stdout logging
    streams.push({
        type: 'raw',
        level: app.rpc ? 'trace' : 'info',
        stream: { write: function(log) {
            if (app.rpc)
                app.rpc.send('log', log);
            else
                process.stdout.write(formatConsole(log));
        } }
    });

    // File logging
    streams.push({
        type: 'rotating-file',
        level: 'debug',
        path: path.join(userPaths.logPath(), 'main-log.jsonl')
    });

    // Log emitter
    streams.push({
        type: 'raw',
        level: 'info',
        stream: { write: function(log) {
            app.emit('log', log);
        } }
    });

    // Create the logger
    app.log = bunyan.createLogger({
        name: "p1stream",
        streams: streams
    });

    // Say hi!
    var version = require('../../package.json').version;
    app.log.info("P1stream v%s starting", version);
};

function formatConsole(log) {
    var time = log.time;
    if (typeof(time) !== 'object')
        time = new Date(time);
    time = isNaN(time.valueOf()) ? '--' : time.toLocaleTimeString();

    var levelNum = log.level;
    var level = nameFromLevel[levelNum] || 'LVL' + levelNum;

    if (levelNum > 40)
        level = chalk.bold.red(level);
    else if (levelNum > 30)
        level = chalk.bold.yellow(level);
    else if (levelNum > 20)
        level = chalk.bold.white(level);
    else
        level = chalk.bold.gray(level);

    var s = time + ' ' + level;
    if (log.obj)
        s += ' ' + chalk.cyan(log.obj);

    var extra = _.omit(log,
        'v', 'time', 'name', 'obj', 'hostname',
        'pid', 'level', 'msg', 'err'
    );

    var msg = log.msg;
    var err = log.err;
    if (err) {
        if (err.stack) {
            msg += '\r\n' + chalk.gray(err.stack);
            err = _.omit(err, 'stack', 'message', 'type', 'name', 'arguments');
        }
        if (_.size(err) !== 0)
            extra.err = _.clone(err);
    }

    if (_.size(extra) !== 0)
        msg += '\r\n' + chalk.gray(util.inspect(extra, { depth: 10 }));

    return s + ' ' + msg + '\r\n';
}

var nameFromLevel = {
    10: 'TRACE',
    20: 'DEBUG',
    30: 'INFO',
    40: 'WARN',
    50: 'ERROR',
    60: 'FATAL'
};
