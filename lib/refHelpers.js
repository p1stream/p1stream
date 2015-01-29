// Helpers for refs on other objects.

var _ = require('underscore');

module.exports = function(scope, attr, id) {
    return {
        add: function(obj) {
            var map = obj[attr];
            var current = map[id];
            map[id] = current ? current + 1 : 1;
            scope.$mark();
        },

        remove: function(obj) {
            var map = obj[attr];
            var current = map[id];
            if (current === 1)
                delete map[id];
            else
                map[id] = current - 1;
            scope.$mark();
        },

        resolve: function(type, watchFn, changeFn) {
            var ref = this;

            var cfgProp = type + 'Id';  // 'thingId' in config
            var resProp = '$' + type;  // resolved '$thing'
            var map = scope[type + 's'];  // 'things' index

            scope.$watchValue(watchFn || function() {
                return _.findWhere(map, { $id: scope.cfg[cfgProp] });
            }, function(current, last) {
                if (last) ref.remove(last);
                if (current) ref.add(current);
                scope[resProp] = current;
                if (changeFn) changeFn(current, last);
                scope.$mark();
            });

            scope.$on('$destroy', function() {
                if (scope[resProp])
                    ref.remove(scope[resProp]);
            });
        },

        resolveAll: function(type, watchFn, changeFn) {
            var ref = this;

            var cfgProp = type.slice(0, -1) + 'Ids';  // 'thingIds' in config
            var resProp = '$' + type;  // resolved '$things'
            var map = scope[type];  // 'things' index

            scope[resProp] = [];

            scope.$watchArray(watchFn || function() {
                return _.compact(_.map(scope.cfg[cfgProp], function(id) {
                    return _.findWhere(map, { $id: id });
                }));
            }, function(current, last) {
                _.each(last, ref.remove);
                _.each(current, ref.add);
                scope[resProp] = current;
                if (changeFn) changeFn(current, last);
                scope.$mark();
            });

            scope.$on('$destroy', function() {
                _.each(scope[resProp], ref.remove);
            });
        }
    };
};
