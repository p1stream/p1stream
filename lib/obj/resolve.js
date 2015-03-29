var _ = require('lodash');

// Resolve a reference to another object. A property `_thing` will be set to
// the actual object based on `getFn`, which should return either an ID or the
// other object. If `getFn` is not set, will look for `thingId` in the config.
// The third parameter is an optional change listener.
exports.resolve = function(name, getFn, changeFn) {
    var self = this;

    var resProp = '_' + name;
    self[resProp] = null;

    if (!getFn) {
        var cfgProp = name + 'Id';
        getFn = function() {
            return self.cfg[cfgProp];
        };
    }

    self.watchValue(function() {
        var other = getFn();
        return typeof(other) === 'string' ? self._store.get(other) : other;
    }, function(current, last) {
        if (last)
            last.unref(self);
        if (current)
            current.ref(self);

        self[resProp] = current;
        self._app.mark();

        if (changeFn)
            changeFn(current, last);
    });

    self.on('destroy', function() {
        var other = self[resProp];
        if (other)
            other.unref(self);
    });
};

// Resolve a list of references to other objects. A property `_things` will be
// set to an array with the actual objects based on `getFn`, which should
// return an array of IDs or the other objects. If `getFn` is not set, will
// look for `thingIds` in the config. The third parameter is an optional change
// listener.
exports.resolveAll = function(name, getFn, changeFn) {
    var self = this;

    var resProp = '_' + name;
    self[resProp] = [];

    if (!getFn) {
        var cfgProp = name.slice(0, -1) + 'Ids';
        getFn = function() {
            return self.cfg[cfgProp];
        };
    }

    self.watchValue(function() {
        return _(getFn()).map(function(other) {
            return typeof(other) === 'string' ? self._store.get(other) : other;
        }).compact().value();
    }, function(current, last) {
        _.each(last, function(other) {
            other.unref(self);
        });
        _.each(current, function(other) {
            other.ref(self);
        });

        self[resProp] = current;
        self._app.mark();

        if (changeFn)
            changeFn(current, last);
    }, 1);

    self.on('destroy', function() {
        _.each(self[resProp], function(other) {
            other.unref(self);
        });
    });
};
