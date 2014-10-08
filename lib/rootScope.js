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
    $watch: function(watcher) {
        var watchers = this.$$watchers;
        watchers.push(watcher);

        this.$mark();

        return function() {
            var idx = watchers.indexOf(watcher);
            watchers.splice(idx, 1);
        };
    },

    // Watcher that watches a value on each change.
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
        this.$$post.push(fn);
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
        var self = this;

        if (self.$root !== self)
            return self.$root.$mark();

        // Check if a cycle is already pending.
        if (self.$$currentMark > self.$$lastMark) return;
        self.$$currentMark++;

        // Check TTL.
        if (self.$$currentMark > self.$$ttl)
            throw new Error("Watcher loop exceeded " + self.$$ttl + " iterations");

        process.nextTick(function() {
            self.$$lastMark = self.$$currentMark;

            // Call watchers.
            self.$$walk(function(scope) {
                scope.$$watchers.slice(0).forEach(function(watcher) {
                    watcher(scope);
                });
            });

            // If not marked again, finish up.
            if (self.$$currentMark === self.$$lastMark) {
                self.$$currentMark = self.$$lastMark = 0;
                self.$$walk(function(scope) {
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
    if (typeof(exp) === 'function') return exp;
    /* jshint -W054 */
    return new Function('$$scope', 'return $$scope.' + exp);
    /* jshint +W054 */
};
