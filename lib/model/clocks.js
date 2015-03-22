var _ = require('lodash');
var native = require('../../build/Release/native.node');
var activation = require('../activation');

module.exports = function(scope) {
    // Define a generic clock category.
    scope.o.$onCreate('clock:', function(obj) {
        // Add activation methods.
        activation(obj, 'Clock');

        // Track the number of mixers are referencing the clock that are active
        // or intend to activate and are waiting for us.
        obj.$watchValue(function() {
            return _.filter(obj.$refs, function(count, id) {
                var other = scope.o[id];
                return other && other.cfg.type === 'mixer' && (
                    other.$activeClock === obj || (
                        !other.hasError && other.numFrameListeners
                    )
                );
            }).length;
        }, function(numConsumers) {
            obj.numConsumers = numConsumers;
            obj.$mark();
        });

        // The default condition. Clock should have consumers and not be in an
        // error state.
        obj.$defaultCond = function() {
            return !obj.hasError && obj.numConsumers;
        };
    });

    // Define the software clock type.
    scope.o.$onCreate('clock:p1stream:software-clock', function(obj) {
        obj.$instance = null;

        obj.$activation({
            start: function() {
                obj.$instance = new native.SoftwareClock({
                    numerator: obj.cfg.numerator || 1,
                    denominator: obj.cfg.denominator || 30
                });
                obj.$mark();
            },
            stop: function() {
                obj.$instance.destroy();
                obj.$instance = null;
                obj.$mark();
            }
        });
    });
};
