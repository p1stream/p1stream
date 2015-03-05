var _ = require('underscore');
var path = require('path');
var util = require('util');
var chalk = require('chalk');
var bunyan = require('bunyan');
var userPaths = require('../userPaths');
var native = require('../../build/Release/native.node');

module.exports = function(scope) {
    var streams = [];

    // Stdout logging
    streams.push({
        type: 'raw',
        level: scope.rpc ? 'trace' : 'info',
        stream: { write: function(log) {
            if (scope.rpc)
                scope.rpc.send('log', log);
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

    // Create the logger
    scope.log = bunyan.createLogger({
        name: "p1stream",
        streams: streams
    });

    // Create a logger per object.
    scope.o.$onCreate(function(obj) {
        obj.$log = scope.log.child({ obj: obj.$id });
    });

    // Helper method to handle standard events.
    scope.handleNativeEvent = function(obj, id, arg) {
        switch (id) {
            case native.EV_LOG_TRACE: obj.$log.trace(arg); break;
            case native.EV_LOG_DEBUG: obj.$log.debug(arg); break;
            case native.EV_LOG_INFO:  obj.$log.info(arg);  break;
            case native.EV_LOG_WARN:  obj.$log.warn(arg);  break;
            case native.EV_LOG_ERROR: obj.$log.error(arg); break;
            case native.EV_LOG_FATAL: obj.$log.fatal(arg); break;

            case native.EV_STALLED:
                obj.$log.warn("Native event buffer dropped %d events during stall", arg);
                break;
        }
    };
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
