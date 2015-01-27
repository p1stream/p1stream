var _ = require('underscore');
var clone = require('clone');

function dummy() {}

var proto = {
    // Create a child scope of this scope.
    $new: function() {
        var child = Object.create(this, {
            $parent: { value: this },
            $$prevSibling: { value: null, writable: true },
            $$nextSibling: { value: this.$$childHead, writable: true }
        });
        this.$$childHead = child;
        init(child);
        return child;
    },

    // Destroy this scope, and its children.
    $destroy: function() {
        Object.defineProperty(this, '$$destroyed', { value: true });
        if (this.$$prevSibling)
            this.$$prevSibling.$$nextSibling = this.$$nextSibling;
        if (this.$$nextSibling)
            this.$$nextSibling.$$prevSibling = this.$$prevSibling;
        if (this.$parent.$$childHead === this)
            this.$parent.$$childHead = this.$$nextSibling;
    },

    // Add a watcher function. A watcher runs everytime the scope is marked.
    // The return value is a function that cancels the watcher.
    $watch: function(fn) {
        var watchers = this.$$watchers;
        watchers.push(fn);

        this.$mark();

        return function() {
            var idx = watchers.indexOf(fn);
            if (idx !== -1)
                watchers.splice(idx, 1);
        };
    },

    // Watcher that watches a value for changes. Can optionally do deep
    // comparisons by specifying a depth as the third parameter. The return
    // value is a function that cancels the watcher.
    $watchValue: function(valueFn, listenFn, depth) {
        valueFn = exports.parse(valueFn);
        var lastValue = dummy;
        return this.$watch(function(scope) {
            var value = valueFn(scope);
            if (!_.isEqual(value, lastValue)) {
                listenFn(value, lastValue, scope);
                lastValue = clone(value, true, depth || 0);
            }
        });
    },

    // Watcher that shallow watches an array for changes.
    // The return value is a function that cancels the watcher.
    $watchArray: function(valueFn, listenFn) {
        valueFn = exports.parse(valueFn);
        var lastValue = dummy;
        var wasArray = false;
        return this.$watch(function(scope) {
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
    },

    // Listen for an event.
    // The return value is a function that cancels the listener.
    $on: function(name, fn) {
        var listeners = this.$$listeners[name] ||
            (this.$$listeners[name] = []);
        listeners.push(fn);

        return function() {
            var idx = listeners.indexOf(fn);
            if (idx !== -1)
                listeners.splice(idx, 1);
        };
    },

    // Emit an event.
    $broadcast: function(name, arg) {
        this.$$walk(function(scope) {
            var listeners = scope.$$listeners[name];
            if (listeners) {
                listeners.slice(0).forEach(function(fn) {
                    fn(arg, scope);
                });
            }
        });
    },

    // Walk this scope and child scopes, depth first.
    $$walk: function(fn) {
        fn(this);

        var child = this.$$childHead;
        while (child) {
            child.$$walk(fn);
            child = child.$$nextSibling;
        }
    },

    // Run watchers until the scope is no longer marked.
    $mark: function() {
        var root = this.$root;

        // Check if a cycle is already pending.
        if (root.$$currentMark > root.$$lastMark) return;
        root.$$currentMark++;

        // Check TTL.
        if (root.$$currentMark > root.$$ttl)
            throw new Error("Watcher loop exceeded " + root.$$ttl + " iterations");

        // Schedule watchers.
        process.nextTick(function() {
            root.$$lastMark = root.$$currentMark;

            // Call watchers.
            root.$$walk(function(scope) {
                scope.$$watchers.slice(0).forEach(function(fn) {
                    fn(scope);
                });
            });

            // If not marked again, finish up.
            if (root.$$currentMark === root.$$lastMark) {
                root.$$currentMark = root.$$lastMark = 0;
                root.$broadcast('$postDigest');
            }
        });
    }
};

// Create a new root scope.
exports = module.exports = function(ttl) {
    var scope = Object.create(proto);
    Object.defineProperties(scope, {
        $root: { value: scope },
        $ttl: { value: ttl || 10 },
        $$currentMark: { value: 0, writable: true },
        $$lastMark: { value: 0, writable: true }
    });
    init(scope);
    return scope;
};
function init(scope) {
    Object.defineProperties(scope, {
        $$watchers: { value: [], writable: true, },
        $$listeners: { value: Object.create(null), writable: true },
        $$childHead: { value: null, writable: true }
    });
    scope.$mark();
}

// Parse a string watcher.
exports.parse = function(exp) {
    if (typeof exp === 'function') return exp;
    // jshint -W054
    return new Function('$$scope', 'return $$scope.' + exp);
    // jshint +W054
};