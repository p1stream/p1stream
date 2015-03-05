var path = require('path');
var http = require('http');
var express = require('express');

module.exports = function(scope) {
    // Main express application.
    scope.app = express();
    // Main HTTP server.
    scope.server = http.createServer(scope.app);

    // Request logging
    scope.app.use(function(req, res, next) {
        var isApiRequest = (req.url.slice(0, 5) === '/api/');

        var writeHead = res.writeHead;
        res.writeHead = function(a, b, c) {
            res.writeHead = writeHead;
            res.writeHead(a, b, c);

            var level = (res.statusCode >= 400) ? 'warn' :
                        (isApiRequest ? 'info' : 'debug');
            scope.log[level]("%s - %s %s - %s", req.socket.remoteAddress,
                req.method, req.url, res.statusCode);
        };

        next();
    });

    // Static serving of web directory.
    scope.$on('preInit', function() {
        var docroot = path.join(__dirname, '..', '..', 'web');
        scope.app.use(express.static(docroot));
    });
};
