var symmetry = require('symmetry');
var bodyParser = require('body-parser');
var eventSource = require('../eventSource');

var symmetryOptions = {
    symmetry: true,
    filter: function(val, key) {
        if (key.charAt(0) !== '_')
            return val;
    }
};

module.exports = function(app) {
    // Last serialized copy of objects.
    var lastData;
    // Whether a send is pending.
    var pending = false;
    // Active event sources.
    var sources = [];

    // Retrieve objects.
    app.get('/api/objects', function(req, res, next) {
        res.send(lastData);
    });

    // Retrieve and monitor objects, using server-sent events.
    app.get('/api/objects.sse', function(req, res, next) {
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
    app.on('postDigest', function() {
        app.emit('dataChanged');
    });
    app.on('dataChanged', function() {
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
        var ret = {};
        for (var obj of app.store.map.values())
            ret[obj.id] = symmetry.cloneJson(obj, symmetryOptions);
        return ret;
    }
};
