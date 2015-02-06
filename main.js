#!/usr/bin/env iojs

var p1stream = require('./');
var port = process.env.PORT || 53311;

var scope = p1stream();
scope.server.listen(port, '127.0.0.1', function() {
    var addr = scope.server.address();
    console.log('Listening on http://' + addr.address + ':' + addr.port + '/');
});
