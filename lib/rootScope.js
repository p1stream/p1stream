var _ = require('underscore');

function dummy() {}

exports = module.exports = function(ttl) {
    if (ttl === undefined) ttl = 10;
    var scope = {};
    var currentMark = 0;
    var lastMark = 0;
    var watchers = [];
    var callbacks = [];

    // Run watchers until the scope is no longer marked.
    function mark() {
        if (currentMark > lastMark) return;
        currentMark++;

        if (currentMark > ttl)
            throw new Error("Watcher loop exceeded " + ttl + " iterations");

        process.nextTick(function() {
            lastMark = currentMark;
            watchers.slice(0).forEach(function(watcher) {
                watcher(scope);
            });
            if (currentMark > lastMark) return;

            currentMark = lastMark = 0;

            callbacks.forEach(function(callback) {
                callback(scope);
            });
        });
    }

    // Add a watcher function. A watcher runs everytime the scope is marked.
    // The return value is a function that cancels the watcher.
    function watch(watcher) {
        watchers.push(watcher);
        scope.$mark();
        return function() {
            var idx = watchers.indexOf(watcher);
            watchers.splice(idx, 1);
        };
    }

    // Watcher that checks a value on each change.
    function watchValue(valueFn, listenFn) {
        valueFn = exports.parse(valueFn);
        var lastValue = dummy;
        return watch(function(scope) {
            var value = valueFn(scope);
            if (!Object.is(value, lastValue)) {
                listenFn(value, lastValue, scope);
                lastValue = value;
            }
        });
    }

    // Watcher that shallow watches an array for changes.
    function watchArray(valueFn, listenFn) {
        valueFn = exports.parse(valueFn);
        var lastValue = dummy;
        var wasArray = false;
        return watch(function(scope) {
            var dirty;

            var value = valueFn(scope);
            var isArray = Array.isArray(value);

            if (isArray && wasArray) {
                var length = value.length;
                dirty = lastValue.length !== length || 
                    value.some(function(el, i) {
                        return !Object.is(el, lastValue[i]);
                    });
            }
            else {
                dirty = !Object.is(value, lastValue);
            }

            if (dirty) {
                listenFn(value, lastValue, scope);
                lastValue = isArray ? value.slice(0) : value;
                wasArray = isArray;
            }
        });
    }

    // Function that runs after the digest cycle finishes.
    function postDigest(callback) {
        callbacks.push(callback);
    }

    Object.defineProperties(scope, {
        $mark: { value: mark },
        $watch: { value: watch },
        $watchValue: { value: watchValue },
        $watchArray: { value: watchArray },
        $postDigest: { value: postDigest }
    });

    scope.$mark();
    return scope;
};

exports.parse = function(exp) {
    if (typeof(exp) === 'function') return exp;
    /* jshint -W054 */
    return new Function('$$scope', 'return $$scope.' + exp);
    /* jshint +W054 */
};
