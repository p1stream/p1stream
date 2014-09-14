module.exports = function(app) {
    var eventSource = require('../eventSource');

    // Retrieve data.
    app.get('/api/data', function(req, res, next) {
        res.send(app.data.$last);
    });

    // Retrieve and monitor data, using server-sent events.
    app.get('/api/data.sse', function(req, res, next) {
        var source = eventSource(res);
        source.send('reset', app.data.$last);

        function onDigest(digest) {
            if (digest === 'reset')
                source.send('reset', app.data.$last);
            else
                source.send('patch', digest);
        }

        app.addListener('digest', onDigest);
        source.on('close', function() {
            app.removeListener('digest', onDigest);
        });
    });
};
