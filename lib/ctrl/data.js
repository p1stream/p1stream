module.exports = function(scope) {
    var eventSource = require('../eventSource');

    // Retrieve data.
    scope.app.get('/api/data', function(req, res, next) {
        res.send(scope.data.$last);
    });

    // Retrieve and monitor data, using server-sent events.
    scope.app.get('/api/data.sse', function(req, res, next) {
        var source = eventSource(res);
        source.send('reset', scope.data.$last);

        function onDigest(digest) {
            if (digest === 'reset')
                source.send('reset', scope.data.$last);
            else
                source.send('patch', digest);
        }

        scope.app.addListener('digest', onDigest);
        source.on('close', function() {
            scope.app.removeListener('digest', onDigest);
        });
    });
};
