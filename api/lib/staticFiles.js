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
    var map = Object.create(null);
    if (base.slice(-1) === '/')
        base = base.slice(0, -1);

    if (base)
        map[base] = { redir: base + '/' };
    else
        base = '/';

    function walk(subdir) {
        var dir = path.join(docroot, subdir);
        fs.readdirSync(dir).forEach(function(entry) {
            var from = path.join(base, subdir, entry);
            var to = path.join(dir, entry);
            var stat = fs.statSync(to);
            if (stat.isFile()) {
                var f = map[from] = {
                    file: to,
                    size: stat.size,
                    mime: mimeTypes[path.extname(to)] ||
                          'application/octet-stream'
                };
                if (entry === 'index.html')
                    map[from.slice(0, -10)] = f;
            }
            else if (stat.isDirectory()) {
                map[from] = { redir: from + '/' };
                walk(path.join(subdir, entry));
            }
        });
    }
    walk('');

    return function(req, res, next) {
        if (req.method === 'GET') {
            var f = map[req.parsed.pathname];
            if (!f)
                return next();

            if (f.redir) {
                res.writeHead(301, {
                    'Location': f.redir,
                    'Content-Type': 'text/plain',
                    'Content-Length': 0
                });
                res.end();
                return;
            }

            var stream = fs.createReadStream(f.file);
            res.writeHead(200, {
                'Content-Type': f.mime,
                'Content-Length': f.size
            });
            stream.pipe(res);
        }
    };
};
