#!/usr/bin/env iojs

var p1stream = require('./');
var urllib = require('url');
var port = process.env.PORT || 53311;

var scope = p1stream();

if (process.argv[2] === '--rpc') {
    scope.rpc = require('jmsg').streams(process.stdin, process.stdout);
    process.stdin.on('end', function() {
        scope.log.info("STDIN closed in RPC mode, stopping.");
        setTimeout(function() {
            process.exit(0);
        }, 1000);
    });
}

scope.server.listen(port, '127.0.0.1', function() {
    var addr = scope.server.address();
    var url = urllib.format({
        protocol: 'http',
        hostname: addr.address,
        port: addr.port,
        pathname: '/'
    });
    scope.log.info('Listening on %s', url);
    if (scope.rpc)
        scope.rpc.send('started', { url: url });
});
