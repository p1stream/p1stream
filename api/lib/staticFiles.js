var fs = require('fs');
var path = require('path');

var mimeTypes = {
    '.html': 'text/html',
    '.css': 'text/css',
    '.js': 'application/javascript'
};

// Static file serving middleware.
// `base` is the base request path.
// `docroot` is the file system path.
module.exports = function(base, docroot) {
    if (typeof(base) === 'string') {
        if (base.slice(-1) === '/')
            base = base.slice(0, -1);
        base = new RegExp('^' + base);
    }

    return function(req, res, next) {
        var p = req.parsed.pathname;
        if (req.method !== 'GET' || !base.test(p))
            return next();

        p = path.resolve('/', decodeURI(p));
        if (p.slice(-1) === '/')
            p += 'index.html';

        var fp = path.join(docroot, p);
        fs.stat(fp, function(err, stats) {
            if (err)
                return next(err.code === 'ENOENT' ? null : err);

            if (stats.isDirectory()) {
                res.writeHead(301, {
                    'Location': p + '/',
                    'Content-Length': 0
                });
                res.end();
                return;
            }

            var stream = fs.createReadStream(fp);
            stream.on('error', next);
            stream.on('open', function(fd) {
                var ext = path.extname(p);
                var mime = mimeTypes.hasOwnProperty(ext) ?
                    mimeTypes[ext] : 'application/octet-stream';
                res.writeHead(200, {
                    'Content-Type': mime,
                    'Content-Length': stats.size
                });
                stream.pipe(res);
            });
        });
    };
};
