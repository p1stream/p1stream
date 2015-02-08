// Create the application object.

var path = require('path');
var http = require('http');
var express = require('express');
var rootScope = require('./rootScope');
var objStore = require('./objStore');
var plugins = require('./plugins');

module.exports = function() {
    // Root model scope.
    var scope = rootScope();
    // Main express application.
    scope.app = express();
    // Main HTTP server.
    scope.server = http.createServer(scope.app);
    // Main object store.
    scope.o = objStore(scope);

    // Load all services, models and controllers.
    require('./svc/config')(scope);
    require('./svc/mkvMuxer')(scope);

    require('./model/roots')(scope);
    require('./model/sources')(scope);
    require('./model/scenes')(scope);
    require('./model/mixers')(scope);

    require('./ctrl/object')(scope);
    require('./ctrl/mixer')(scope);

    // Load all plugins.
    var pluginNames = plugins.pluginNames();
    pluginNames.forEach(function(pluginName) {
        plugins.loadPlugin(pluginName)(scope);
    });

    // Static serving of web directory.
    var docroot = path.join(__dirname, '..', 'web');
    scope.app.use(express.static(docroot));

    // Load state from configuration.
    scope.initFromConfig();

    // Expose list of loaded plugins on the root object.
    scope.o['root:p1stream'].pluginNames = pluginNames;

    return scope;
};
