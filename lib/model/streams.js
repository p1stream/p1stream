var activation = require('../activation');

module.exports = function(scope) {
    // Define a generic stream category.
    scope.o.$onCreate('stream:', function(obj) {
        // Add activation methods.
        activation(obj, 'Stream');

        // The default condition. Start if the active flag is set.
        obj.$defaultCond = function() {
            return !obj.hasError && obj.$mixer && obj.active;
        };

        // Apply configuration autostart flag.
        obj.active = obj.cfg.autostart;
    });
};
