var _ = require('lodash');

module.exports = function(app) {
    // Define a root category. These are always sticky and usually singleton,
    // meaning each subtype only has one instance, where the ID is the same as
    // the type name.
    //
    // Each plugin should create a root to store its settings, reference its
    // other objects and expose any other useful 'global' runtime data to web
    // clients.
    app.store.onCreate('root:', function(obj) {
        obj._sticky = true;
    });

    // The P1stream root. This references scenes and mixers,
    // and exposes a ring buffer of log messages.
    app.store.onCreate('root:p1stream', function(obj) {
        obj.resolveAll('scenes');
        obj.resolveAll('mixers');

        var logs = obj.logs = [];
        app.on('log', function(log) {
            logs.push(log);
            if (logs.length > 1000)
                logs.shift();
            app.emit('dataChanged');
        });
    });

    // Set config defaults for `root:p1stream` before init.
    app.on('preInit', function() {
        var settings = app.cfg['root:p1stream'] ||
            (app.cfg['root:p1stream'] = {});
        _.defaults(settings, {
            type: 'root:p1stream',
            sceneIds: [],
            mixerIds: []
        });
    });
};
