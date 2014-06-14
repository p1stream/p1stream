var url = require('url');
var http = require('http');

// Simple middleware layer.
exports.create = function() {
    var middleware = [];

    var app = function(req, res) {
        req.parsed = url.parse(req.url);
        res.send = send;

        var i = 0;
        function next(err) {
            if (err) {
                res.send(500, err.stack);
                return;
            }

            var fn = middleware[i++];
            if (!fn) {
                res.send(400, 'Cannot ' + req.method + ' ' +
                              req.parsed.pathname + '\n');
                return;
            }

            fn(req, res, next);
        }
        next();
    };

    app.use = function(fn) {
        middleware.push(fn);
    };

    ['get', 'post', 'put', 'delete'].forEach(function(method) {
        var httpMethod = method.toUpperCase();
        app[method] = function(re, fn) {
            app.use(function(req, res, next) {
                var match = req.method === httpMethod &&
                            re.exec(req.parsed.pathname);
                if (match)
                    fn(req, res, next, match);
                else
                    next();
            });
        };
    });

    app.listen = function() {
        var server = http.createServer(app);
        server.listen.apply(server, arguments);
        return server;
    };

    return app;
};

// `req.send` helper.
function send(code, data) {
    var contentType = 'application/octet-stream';
    if (typeof(data) === 'string') {
        contentType = 'text/plain; charset=utf-8';
        data = new Buffer(data);
    }
    else if (!Buffer.isBuffer(data)) {
        contentType = 'application/json';
        data = new Buffer(JSON.stringify(data));
    }

    this.writeHead(code, {
        'Content-Type': contentType,
        'Content-Length': data.length
    });
    this.end(data);
}
