var _ = require('underscore');

function dummy() {}

var proto = {
    // Create a child scope of this scope.
    $new: function() {
        var child = Object.create(this);
        Object.defineProperties(child, {
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

    // Watcher that watches a value for changes.
    $watchValue: function(valueFn, listenFn) {
        valueFn = exports.parse(valueFn);
        var lastValue = dummy;
        return this.$watch(function(scope) {
            var value = valueFn(scope);
            if (!Object.is(value, lastValue)) {
                listenFn(value, lastValue, scope);
                lastValue = value;
            }
        });
    },

    // Watcher that shallow watches an array for changes.
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

    // Function that runs after a cycle finishes.
    $post: function(fn) {
        var post = this.$$post;
        post.push(fn);

        return function() {
            var idx = post.indexOf(fn);
            if (idx !== -1)
                post.splice(idx, 1);
        };
    },

    // Walk this scope and child scopes.
    $$walk: function(fn) {
        var current = this;
        while (true) {
            fn(current);

            while (current !== this && current.$$destroyed)
                current = current.$parent;

            if (current.$$childHead) {
                current = current.$$childHead;
                continue;
            }

            while (current !== this && !current.$$nextSibling)
                current = current.$parent;

            if (current === this) break;
            current = current.$$nextSibling;
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
                scope.$$watchers.slice(0).forEach(function(watcher) {
                    watcher(scope);
                });
            });

            // If not marked again, finish up.
            if (root.$$currentMark === root.$$lastMark) {
                root.$$currentMark = root.$$lastMark = 0;
                root.$$walk(function(scope) {
                    scope.$$post.forEach(function(fn) {
                        fn(scope);
                    });
                });
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
        $$post: { value: [], writable: true },
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
