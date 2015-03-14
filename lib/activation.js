// Common logic for activatable object types.
module.exports = function(obj, logType) {
    // Convenience method to install activation watchers. The parameters
    // object must contain `start` and `stop` functions. The `cond`
    // parameter can be set to override the default activation condition.
    obj.$activation = function(params) {
        var running = false;

        obj.$watchValue(params.cond || obj.$defaultCond, function(val) {
            val = !!val;
            if (running !== val) {
                running = val;
                if (val) {
                    obj.$log.info("%s activated", logType);
                    params.start();
                }
                else {
                    obj.$log.info("%s deactivated", logType);
                    params.stop();
                }
            }
        });

        obj.$on('$destroy', function() {
            if (running)
                params.stop();
        });
    };

    // Clear the error flag, allowing the mixer to start again.
    obj.$clearError = function() {
        obj.hasError = false;
        obj.$mark();
    };

    // Sets the error flag, and logs any arguments given.
    obj.$fatal = function() {
        if (arguments.length)
            obj.$log.error.apply(obj.$log, arguments);
        obj.hasError = true;
        obj.$mark();
    };
};
