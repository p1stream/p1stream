var symmetry = require('symmetry');

module.exports = function(digestCb) {
    var data = symmetry.scope();
    var currentMark = 0;
    var lastMark = 0;
    var watchers = [];

    // Install/uninstall watchers.
    function watch(fn) {
        var idx = watchers.indexOf(fn);
        if (idx === -1)
            watchers.push(fn);
    }
    function unwatch(fn) {
        var idx = watchers.indexOf(fn);
        if (idx !== -1)
            watchers.splice(idx, 1);
    }

    // Run watchers, emit an update.
    function mark() {
        if (currentMark > lastMark) return;
        currentMark++;

        if (currentMark > 10)
            throw new Error("Watcher loop exceeded 10 iterations");

        process.nextTick(function() {
            lastMark = currentMark;
            watchers.forEach(function(fn) {
                fn();
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
        $unwatch: unwatch,
        $mark: mark
    });

    data.$mark();
    return data;
};
