var _ = require('underscore');
var activation = require('../activation');

module.exports = function(scope) {
    // Define a generic source category.
    scope.o.$onCreate('source:', function(obj) {
        // Add activation methods.
        activation(obj, 'Source');

        // Track the number of active mixers that are referencing the source,
        // which can be used for automatic activation.
        obj.$watchValue(function() {
            return _.filter(obj.$refs, function(count, id) {
                var other = scope.o[id];
                return other && other.cfg.type === 'mixer' && other.$instance;
            }).length;
        }, function(numConsumers) {
            obj.numConsumers = numConsumers;
            obj.$mark();
        });

        // The default condition. Source should have consumers or be set to
        // prewarm, and not be in an error state.
        obj.$defaultCond = function() {
            return !obj.hasError && (obj.numConsumers || obj.cfg.warm);
        };
    });
};
