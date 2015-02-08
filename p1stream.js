#!/usr/bin/env iojs

var p1stream = require('./');
var urllib = require('url');
var port = process.env.PORT || 53311;

var rpc;
if (process.argv[2] === '--rpc') {
    rpc = require('jmsg').streams(process.stdin, process.stdout);
    process.stdin.on('end', function() {
        process.stderr.write("STDIN closed, stopping.\n", function() {
            process.exit(0);
        });
    });
}

var scope = p1stream();
scope.server.listen(port, '127.0.0.1', function() {
    var addr = scope.server.address();
    var url = urllib.format({
        protocol: 'http',
        hostname: addr.address,
        port: addr.port,
        pathname: '/'
    });
    if (rpc)
        rpc.send('started', { url: url });
    else
        console.log('Listening on ' + url);
});
