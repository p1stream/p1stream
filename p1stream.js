#!/usr/bin/env iojs

require('./lib/platform').setup(function() {
    var p1stream = require('./');
    var urllib = require('url');
    var port = process.env.PORT || 53311;

    var rpc, app;

    if (process.argv[2] === '--rpc') {
        rpc = require('jmsg').streams(process.stdin, process.stdout);
        process.stdin.on('end', function() {
            app.log.info("STDIN closed in RPC mode, stopping.");
            setTimeout(function() {
                process.exit(0);
            }, 1000);
        });
    }

    app = p1stream({ rpc: rpc });
    app.server.listen(port, '127.0.0.1', function() {
        var addr = app.server.address();
        var url = urllib.format({
            protocol: 'http',
            hostname: addr.address,
            port: addr.port,
            pathname: '/'
        });
        app.log.info('Listening on %s', url);
        if (app.rpc)
            app.rpc.send('started', { url: url });
    });
});
