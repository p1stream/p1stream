// Create the application object.

var _ = require('lodash');
var http = require('http');
var express = require('express');
var userPaths = require('./userPaths');
var Watchable = require('./watchable');
var plugins = require('./plugins');

module.exports = function(options) {
    // Make sure directories exist.
    userPaths.createWriteableDirsSync();

    // Main express application.
    var app = express();
    // Main HTTP server.
    app.server = http.createServer(app);

    // Make app a watchable.
    _.extend(app, Watchable.prototype);
    Watchable.init(app);

    // Set the RPC object, so it is available to services.
    if (options && options.rpc)
        app.rpc = options.rpc;

    // Load all services, models and controllers.
    require('./svc/logging')(app);
    require('./svc/objects')(app);
    require('./svc/config')(app);
    require('./svc/web')(app);
    require('./svc/util')(app);

    require('./model/roots')(app);
    require('./model/sources')(app);
    require('./model/clocks')(app);
    require('./model/videoMixers')(app);
    require('./model/audioMixers')(app);
    require('./model/scenes')(app);
    require('./model/mixers')(app);
    require('./model/streams')(app);

    require('./ctrl/object')(app);
    require('./ctrl/mixer')(app);

    // Load all plugins.
    var pluginNames = plugins.pluginNames();
    pluginNames.forEach(function(pluginName) {
        plugins.loadPlugin(pluginName)(app);
    });

    // Load state from configuration.
    app.emit('preInit');
    app.initFromConfig();
    app.emit('postInit');

    // Expose list of loaded plugins on the root object.
    app.o('root:p1stream').pluginNames = pluginNames;

    return app;
};
