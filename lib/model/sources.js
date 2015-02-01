var _ = require('underscore');

module.exports = function(scope) {
    // Define a generic source category. Track the number of active mixers that
    // are referencing the source, which can be used for automatic activation.
    scope.o.$onCreate('source:', function(obj) {
        obj.$watchValue(function() {
            return _.filter(obj.$refs, function(count, id) {
                var other = scope.o[id];
                return other && other.cfg.type === 'mixer' && other.$instance;
            }).length;
        }, function(numConsumers) {
            obj.numConsumers = numConsumers;
            obj.$mark();
        });
    });
};
