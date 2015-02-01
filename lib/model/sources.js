var _ = require('underscore');

module.exports = function(scope) {
    // Define a generic source category.
    scope.o.$onCreate('source:', function(obj) {
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

        // Convenience method to install activation watchers. The parameters
        // object must contain `start` and `stop` functions. The `cond`
        // parameter can be set to override the default activation condition.
        obj.$activation = function(params) {
            var running = false;

            obj.$watchValue(params.cond || obj.$defaultCond, function(val) {
                val = !!val;
                if (running !== val) {
                    running = val;
                    if (val)
                        params.start();
                    else
                        params.stop();
                }
            });

            obj.$on('$destroy', function() {
                if (running)
                    params.stop();
            });
        };

        // The default condition. Source should have consumers or be set to
        // prewarm, and not be in an error state.
        obj.$defaultCond = function() {
            return !obj.hasError && (obj.numConsumers || obj.cfg.warm);
        };
    });
};
