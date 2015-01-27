module.exports = function(scope) {
    var eventSource = require('../eventSource');

    // Active sources.
    var sources = [];

    // Retrieve data.
    scope.app.get('/api/data', function(req, res, next) {
        res.send(scope.data.$last);
    });

    // Retrieve and monitor data, using server-sent events.
    scope.app.get('/api/data.sse', function(req, res, next) {
        var source = eventSource(res);
        source.send('reset', scope.data.$last);

        sources.push(source);
        source.on('close', function() {
            var idx = sources.indexOf(source);
            sources.splice(idx, 1);
        });
    });

    // Send patches to all active sources.
    scope.$on('$postDigest', function() {
        var digest = scope.data.$digest();
        if (digest === 'none')
            return;

        sources.forEach(function(source) {
            if (digest === 'reset')
                source.send('reset', scope.data.$last);
            else
                source.send('patch', digest);
        });
    });
};
