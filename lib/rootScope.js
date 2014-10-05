function dummy() {}

module.exports = function(ttl) {
    if (ttl === undefined) ttl = 10;
    var scope = {};
    var currentMark = 0;
    var lastMark = 0;
    var watchers = [];
    var callbacks = [];

    // Watchers that check a value on each change.
    function watch(watchFn, listenerFn) {
        var watcher = {
            lastValue: dummy,
            watchFn: watchFn || dummy,
            listenerFn: listenerFn || dummy
        };
        watchers.push(watcher);
        scope.$mark();
        return function unwatch() {
            var idx = watchers.indexOf(watcher);
            watchers.splice(idx, 1);
        };
    }

    // Run watchers, emit an update.
    function mark() {
        if (currentMark > lastMark) return;
        currentMark++;

        if (currentMark > ttl)
            throw new Error("Watcher loop exceeded " + ttl + " iterations");

        process.nextTick(function() {
            lastMark = currentMark;
            watchers.slice(0).forEach(function(watcher) {
                var lastValue = watcher.lastValue;
                var value = watcher.watchFn(scope);
                if (!Object.is(value, lastValue)) {
                    watcher.listenerFn(value, lastValue, scope);
                    watcher.lastValue = value;
                }
            });
            if (currentMark > lastMark) return;

            currentMark = lastMark = 0;

            callbacks.forEach(function(callback) {
                callback(scope);
            });
        });
    }

    function postDigest(callback) {
        callbacks.push(callback);
    }

    Object.defineProperties(scope, {
        $watch: { value: watch },
        $mark: { value: mark },
        $postDigest: { value: postDigest }
    });

    scope.$mark();
    return scope;
};
