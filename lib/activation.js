var humanizeDuration = require('humanize-duration');

// Common logic for activatable object types.
module.exports = function(obj, logType) {
    // Convenience method to install activation watchers. The parameters
    // object must contain `start` and `stop` functions. The `cond`
    // parameter can be set to override the default activation condition.
    obj.$activation = function(params) {
        obj.activated = false;

        obj.$watchValue(params.cond || obj.$defaultCond, function(val) {
            if (val && obj.activated === false) {
                obj.$log.info("%s activated", logType);
                obj.activated = Date.now();
                params.start();
            }
            else if (!val && obj.activated !== false) {
                obj.$log.info("%s deactivated after %s",
                    logType, humanizeDuration(Date.now() - obj.activated));
                obj.activated = false;
                params.stop();
            }
        });

        obj.$on('$destroy', function() {
            if (obj.actived !== false)
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
