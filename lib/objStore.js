// Simple object store. Each object has an id, type and config. The entire
// object is synchronized to web clients, but only the config part is saved.
//
// Objects can reference eachother, and are garbage collected once all
// references are removed.
//
// Each object also is a child scope. Modules can use onCreate to install
// watchers and add behavior to objects.

var _ = require('underscore');
var uuid = require('node-uuid');

module.exports = function(scope) {
    var store = Object.create(null);
    store.$types = Object.create(null);
    store.$createListeners = [];

    // Create an object.
    store.$create = function(cfg, id) {
        var obj = _.extend(scope.$new(), methods, {
            $id: id || uuid(),
            $refs: Object.create(null),
            $sticky: false,     // Whether to disable GC.
            $ephemeral: false,  // Whether to disable saving to config.
            cfg: cfg
        });

        store[obj.$id] = obj;
        store.$createListeners.forEach(function(fn) {
            fn(obj);
        });

        scope.$mark();
        return obj;
    };

    // Install a callback on create. The type parameter matches on
    // `obj.cfg.type`. If type ends with a semicolon, the remainder is a
    // wildcard, which allows matching categories and subtypes.
    store.$onCreate = function(type, fn) {
        var listener = function(obj) {
            if (type.slice(-1) === ':' ?
                    obj.cfg.type.slice(0, type.length) === type :
                    obj.cfg.type === type)
                fn(obj);
        };

        store.$createListeners.push(listener);
        return function() {
            var idx = store.$createListeners.indexOf(listener);
            if (idx !== -1)
                store.$createListeners.splice(idx, 1);
        };
    };

    var methods = {
        // Destroy the object.
        $destroy: function() {
            delete store[this.$id];
            this.$destroy();
        },

        // Helper to add a reference to another object.
        $addRef: function(other) {
            var current = other.$refs[this.$id];
            other.$refs[this.$id] = current ? current + 1 : 1;
            this.$mark();
        },

        // Helper to remove a reference to another object.
        $removeRef: function(other) {
            var current = other.$refs[this.$id];
            if (current === 1)
                delete other.$refs[this.$id];
            else
                other.$refs[this.$id] = current - 1;

            if (!other.$sticky && _.isEmpty(other.$refs))
                other.$destroy();

            this.$mark();
        },

        // Resolve a reference to another object. A property `$thing` will be
        // set to the actual object based on `getFn`, which should return
        // either an ID or the other object. If `getFn` is not set, will look
        // for `thingId` in the config. The third parameter is an optional
        // change listener.
        $resolve: function(name, getFn, changeFn) {
            var self = this;

            var resProp = '$' + name;
            self[resProp] = null;

            if (!getFn) {
                var cfgProp = name + 'Id';
                getFn = function() {
                    return self.cfg[cfgProp];
                };
            }

            self.$watchValue(function() {
                var other = getFn();
                return typeof(other) === 'string' ? store[other] : other;
            }, function(current, last) {
                if (last)
                    self.$removeRef(last);
                if (current)
                    self.$addRef(current);

                self[resProp] = current;
                self.$mark();

                if (changeFn)
                    changeFn(current, last);
            });

            self.$on('$destroy', function() {
                if (self[resProp])
                    self.$removeRef(self[resProp]);
            });
        },

        // Resolve a list of references to other objects. A property `$things`
        // will be set to an array with the actual objects based on `getFn`,
        // which should return an array of IDs or the other objects. If `getFn`
        // is not set, will look for `thingIds` in the config. The third
        // parameter is an optional change listener.
        $resolveAll: function(name, getFn, changeFn) {
            var self = this;

            var resProp = '$' + name;
            self[resProp] = [];

            if (!getFn) {
                var cfgProp = name.slice(0, -1) + 'Ids';
                getFn = function() {
                    return self.cfg[cfgProp];
                };
            }

            self.$watchArray(function() {
                return _.compact(_.map(getFn(), function(other) {
                    return typeof(other) === 'string' ? store[other] : other;
                }));
            }, function(current, last) {
                _.each(last, self.$removeRef, self);
                _.each(current, self.$addRef, self);

                self[resProp] = current;
                self.$mark();

                if (changeFn)
                    changeFn(current, last);
            });

            self.$on('$destroy', function() {
                _.each(self[resProp], self.$removeRef, self);
            });
        }
    };

    return store;
};