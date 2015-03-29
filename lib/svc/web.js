var _ = require('lodash');
var fs = require('fs');
var path = require('path');
var express = require('express');

module.exports = function(app) {
    // Document root.
    var docRoot = path.join(__dirname, '..', '..', 'web');
    // Index template parameters.
    app.index = { js: [], css: [] };
    // Index template.
    var indexTmpl = _.template(
        fs.readFileSync(path.join(docRoot, 'index.html'), 'utf8'),
        { variable: 'unused', imports: app.index }
    );

    // Request logging
    app.use(function(req, res, next) {
        var isApiRequest = (req.url.slice(0, 5) === '/api/');

        var writeHead = res.writeHead;
        res.writeHead = function(a, b, c) {
            res.writeHead = writeHead;
            res.writeHead(a, b, c);

            var level = (res.statusCode >= 400) ? 'warn' :
                        (isApiRequest ? 'info' : 'debug');
            app.log[level]("%s - %s %s - %s", req.socket.remoteAddress,
                req.method, req.url, res.statusCode);
        };

        next();
    });

    // Static serving of web directory.
    app.on('preInit', function() {
        app.get('/', serveIndex);
        app.get('/index.html', serveIndex);

        app.use(express.static(docRoot));
    });

    function serveIndex(req, res) {
        res.setHeader('Content-Type', 'text/html; charset=utf-8');
        res.end(indexTmpl());
    }
};
