var atomApp = require('app');

var scope;  // Our only global!

atomApp.on('will-finish-launching', function() {
    scope = require('./web')();
});

atomApp.on('ready', function() {
    var BrowserWindow = require('browser-window');
    var server = scope.app.listen(53311, '127.0.0.1', function() {
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

function createApp() {
    var path = require('path');
    var express = require('express');
    var symmetry = require('symmetry');
    var config = require('./config');
    var rootScope = require('./rootScope');

    var scope = rootScope();
    var app = scope.app = express();
    var cfg = scope.cfg = config.loadConfig();
    var data = scope.data = symmetry.scope();

    require('./engine')(scope);

    require('./ctrl/data')(scope);
    require('./ctrl/stream')(scope);

    var docroot = path.join(__dirname, '..', 'web');
    app.use(express.static(docroot));

    data.p = {};  // Plugin namespaces.
    config.pluginNames().forEach(function(pluginName) {
        config.loadPlugin(pluginName)(scope);
    });

    return scope;
}
