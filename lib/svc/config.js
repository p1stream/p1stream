var _ = require('underscore');
var fs = require('fs');
var userPaths = require('../userPaths');

module.exports = function(scope) {
    scope.cfgFile = userPaths.configPath();

    scope.log.info("Reading config file '%s'", scope.cfgFile);
    try {
        scope.cfg = JSON.parse(fs.readFileSync(scope.cfgFile, 'utf8'));
    }
    catch (err) {
        if (err.code !== 'ENOENT')
            scope.log.error(err, "Could not read '%s'", scope.cfgFile);
        scope.cfg = {};
    }
    scope.log.debug("Loaded %d objects", _.size(scope.cfg));

    // Create instances of all objects in configuration.
    // Should be called once after all onCreate listeners are installed.
    scope.initFromConfig = function() {
        scope.$broadcast('preInit');

        scope.log.debug("Initializing %d objects", _.size(scope.cfg));
        _.each(scope.cfg, scope.o.$create);

        scope.$broadcast('postInit');
    };

    // Write the configuration based on current state.
    scope.save = function() {
        // Take configuration from all objects.
        var cfg = scope.cfg = {};
        _.each(scope.o, function(obj, id) {
            if (id[0] !== '$' && !obj.$ephemeral)
                cfg[id] = obj.cfg;
        });

        // Write the file.
        scope.log.info("Writing config file '%s'", scope.cfgFile);
        try {
            var json = JSON.stringify(cfg, function(key, value) {
                return key[0] === '$' ? undefined : value;
            }, 2);
            fs.writeFileSync(scope.cfgFile, json);
        }
        catch (err) {
            scope.log.error(err, "Could not write '%s'", scope.cfgFile);
            return;
        }

        scope.log.debug("Wrote %d objects", _.size(cfg));
    };
};
