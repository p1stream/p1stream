var symmetry = require('symmetry');

module.exports = function(digestCb) {
    var data = symmetry.scope();
    var mark = 0;
    var lastMark = 0;
    var watchers = [];

    // Install/uninstall watchers.
    data.$watch = function(fn) {
        var idx = watchers.indexOf(fn);
        if (idx === -1)
            watchers.push(fn);
    };
    data.$unwatch = function(fn) {
        var idx = watchers.indexOf(fn);
        if (idx !== -1)
            watchers.splice(idx, 1);
    };

    // Run watchers, emit an update.
    data.$mark = function() {
        if (mark > lastMark) return;
        mark++;

        if (mark > 10)
            throw new Error("Watcher loop exceeded 10 iterations");

        process.nextTick(function() {
            lastMark = mark;
            watchers.forEach(function(fn) {
                fn();
            });
            if (mark > lastMark) return;

            mark = lastMark = 0;

            var digest = data.$digest();
            if (digest !== 'none')
                digestCb(digest);
        });
    };

    data.$mark();
    return data;
};
