var symmetry = require('symmetry');

function dummy() {}

module.exports = function(digestCb) {
    var data = symmetry.scope();
    var currentMark = 0;
    var lastMark = 0;
    var watchers = [];

    // Watchers that check a value on each change.
    function watch(watchFn, listenerFn) {
        var watcher = {
            lastValue: dummy,
            watchFn: watchFn || dummy,
            listenerFn: listenerFn || dummy
        };
        watchers.push(watcher);
        data.$mark();
        return function unwatch() {
            var idx = watchers.indexOf(watcher);
            watchers.splice(idx, 1);
        };
    }

    // Run watchers, emit an update.
    function mark() {
        if (currentMark > lastMark) return;
        currentMark++;

        if (currentMark > 10)
            throw new Error("Watcher loop exceeded 10 iterations");

        process.nextTick(function() {
            lastMark = currentMark;
            watchers.slice(0).forEach(function(watcher) {
                var lastValue = watcher.lastValue;
                var value = watcher.watchFn(data);
                if (!Object.is(value, lastValue)) {
                    watcher.listenerFn(value, lastValue, data);
                    watcher.lastValue = value;
                }
            });
            if (currentMark > lastMark) return;

            currentMark = lastMark = 0;

            var digest = data.$digest();
            if (digest !== 'none')
                digestCb(digest);
        });
    }

    Object.defineProperties(data, {
        $watch: watch,
        $mark: mark
    });

    data.$mark();
    return data;
};
