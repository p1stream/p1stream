var events = require('events');

module.exports = function(res) {
    var source = new events.EventEmitter();
    var aliveTimeout = null;

    source.send = function(e, data) {
        scheduleKeepalive();
        res.write('event:' + e + '\n' +
                  'data:' + JSON.stringify(data) + '\n\n');
    };

    source.comment = function(s) {
        scheduleKeepalive();
        res.write(':' + s + '\n\n');
    };

    source.close = function() {
        cancelKeepalive();
        res.end();
    };

    res.useChunkedEncodingByDefault = false;
    res.writeHead(200, {
        'Connection': 'close',
        'Content-Type': 'text/event-stream'
    });
    scheduleKeepalive();

    res.on('close', function() {
        cancelKeepalive();
        source.emit('close');
    });

    function scheduleKeepalive() {
        cancelKeepalive();
        aliveTimeout = setTimeout(sendKeepalive, 20000);
    }

    function cancelKeepalive() {
        if (aliveTimeout) {
            clearTimeout(aliveTimeout);
            aliveTimeout = null;
        }
    }

    function sendKeepalive() {
        aliveTimeout = null;
        source.comment('');
    }

    return source;
};
