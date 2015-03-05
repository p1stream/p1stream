var symmetry = require('symmetry');
var eventSource = require('../eventSource');

var symmetryOptions = {
    symmetry: true,
    filter: symmetry.scopeFilter
};

module.exports = function(scope) {
    // Last serialized copy of objects.
    var lastData;
    // Whether a send is pending.
    var pending = false;
    // Active event sources.
    var sources = [];

    // Retrieve objects.
    scope.app.get('/api/objects', function(req, res, next) {
        res.send(lastData);
    });

    // Retrieve and monitor objects, using server-sent events.
    scope.app.get('/api/objects.sse', function(req, res, next) {
        if (sources.length === 0)
            lastData = serialize();

        var source = eventSource(res);
        source.send('reset', lastData);

        sources.push(source);
        source.on('close', function() {
            var idx = sources.indexOf(source);
            sources.splice(idx, 1);

            if (sources.length === 0)
                lastData = undefined;
        });
    });

    // Send patches to all active sources.
    scope.$on('$postDigest', function() {
        scope.$broadcast('dataChanged');
    });
    scope.$on('dataChanged', function() {
        if (!pending && sources.length !== 0) {
            pending = true;
            process.nextTick(sendChanges);
        }
    });
    function sendChanges() {
        pending = false;

        var current = serialize();
        var digest = symmetry.diffObject(lastData, current, symmetryOptions);
        if (digest === 'none')
            return;
        lastData = current;

        sources.forEach(function(source) {
            if (digest === 'reset')
                source.send('reset', lastData);
            else
                source.send('patch', digest);
        });
    }

    // Helper, create a serialized copy of the object store.
    function serialize() {
        return symmetry.cloneJson(scope.o, symmetryOptions);
    }
};
