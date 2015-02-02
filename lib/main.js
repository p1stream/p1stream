// Main application entry point.

var scope;  // Our only global!

if (process.versions['atom-shell']) {
    var atomApp = require('app');

    // Go away, stupid default atom popup error handler.
    process.removeAllListeners('uncaughtException');

    atomApp.on('will-finish-launching', function() {
        scope = createApp();
    });

    atomApp.on('ready', function() {
        var BrowserWindow = require('browser-window');
        listen(function() {
            var mainWindow = new BrowserWindow({ width: 800, height: 600 });
            mainWindow.loadUrl('http://127.0.0.1:53311/');
            mainWindow.on('closed', function() {
                mainWindow = null;
            });
        });
    });

    atomApp.on('window-all-closed', function() {
        if (process.platform !== 'darwin')
            atomApp.quit();
    });
}
else {
    scope = createApp();
    listen(function() {
        var addr = scope.server.address();
        console.log('Listening on http://' + addr.address + ':' + addr.port + '/');
    });
}

function createApp() {
    var path = require('path');
    var http = require('http');
    var express = require('express');
    var rootScope = require('./rootScope');
    var objStore = require('./objStore');
    var plugins = require('./plugins');

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
}

function listen(cb) {
    scope.server.listen(53311, '127.0.0.1', cb);
}
