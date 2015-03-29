// AngularJS inspired watchers, but with explicit dirty marking.

var _ = require('lodash');
var clone = require('clone');

function Watchable() {
    Watchable.init(this);
}

module.exports = Watchable;

Watchable.defaultDigestTtl = 10;

Watchable.init = function(obj) {
    obj._watchers = [];
    obj._currentMark = 0;
    obj._lastMark = 0;
    obj._digestTtl = Watchable.defaultTtl;
};

// Add a watcher function. A watcher runs everytime the object is marked.
// The return value is a function that cancels the watcher.
Watchable.prototype.watch = function(fn) {
    if (!this._watchers)
        Watchable.init(this);

    var watchers = this._watchers;
    watchers.push(fn);

    this.mark();

    return function() {
        fn._cancelled = true;   // FIXME: unique
        var idx = watchers.indexOf(fn);
        if (idx !== -1)
            watchers.splice(idx, 1);
    };
};

// Watcher that watches a value for changes. Can optionally do deep
// comparisons by specifying a depth as the third parameter. The return
// value is a function that cancels the watcher.
Watchable.prototype.watchValue = function(valueFn, listenFn, depth) {
    var lastValue;
    return this.watch(function() {
        var value = valueFn();
        if (!_.isEqual(value, lastValue)) {
            listenFn(value, lastValue);
            lastValue = clone(value, true, depth || 0);
        }
    });
};

// Run watchers until no longer marked. If the the object is and EventEmitter,
// an event `postDigest` is emitted when complete.
//
// Takes an optional callback that triggers on the next `postDigest` event.
Watchable.prototype.mark = function(digestCb) {
    var self = this;

    // Install optional callback.
    if (digestCb)
        this.once('postDigest', digestCb);

    // Check if a cycle is already pending.
    if (self._currentMark > self._lastMark) return;
    self._currentMark++;

    // Check TTL.
    if (self._currentMark > self._digestTtl)
        throw new Error("Watcher loop exceeded " + self.digestTtl + " iterations");

    // Schedule watchers.
    process.nextTick(function() {
        self._lastMark = self._currentMark;

        // Call watchers.
        self._watchers.slice(0).forEach(function(fn) {
            if (!fn._cancelled)
                fn();
        });

        // If not marked again, finish up.
        if (self._currentMark === self._lastMark) {
            self._currentMark = self._lastMark = 0;
            if (self.emit)
                self.emit('postDigest');
        }
    });
};
