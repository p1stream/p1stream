var _ = require('lodash');
var fs = require('fs');
var userPaths = require('../userPaths');

module.exports = function(app) {
    app.cfgFile = userPaths.configPath();

    app.log.info("Reading config file '%s'", app.cfgFile);
    try {
        app.cfg = JSON.parse(fs.readFileSync(app.cfgFile, 'utf8'));
    }
    catch (err) {
        if (err.code !== 'ENOENT')
            app.log.error(err, "Could not read '%s'", app.cfgFile);
        app.cfg = {};
    }
    app.log.debug("Loaded %d objects", _.size(app.cfg));

    // Create instances of all objects in configuration.
    // Should be called once after all onCreate listeners are installed.
    app.initFromConfig = function() {
        app.log.debug("Initializing %d objects", _.size(app.cfg));
        _.each(app.cfg, app.store.create, app.store);
    };

    // Write the configuration based on current state.
    function actualSave() {
        // Take configuration from all objects.
        var cfg = app.cfg = {};
        app.store.forEach(function(obj, id) {
            if (!obj._ephemeral)
                cfg[id] = obj.cfg;
        });

        // Write the file.
        app.log.info("Writing config file '%s'", app.cfgFile);
        try {
            var json = JSON.stringify(cfg, null, 2);
            fs.writeFileSync(app.cfgFile, json);
        }
        catch (err) {
            app.log.error(err, "Could not write '%s'", app.cfgFile);
            return;
        }

        app.log.debug("Wrote %d objects", _.size(cfg));
    }

    var saveTimeout = null;

    // Immediate save.
    app.save = function() {
        if (saveTimeout) {
            clearTimeout(saveTimeout);
            saveTimeout = null;
        }

        actualSave();
    };

    // Deferred save.
    app.queueSave = function() {
        if (saveTimeout) {
            clearTimeout(saveTimeout);
            saveTimeout = null;
        }

        saveTimeout = setTimeout(function() {
            saveTimeout = null;
            actualSave();
        }, 3000);
    };
};
