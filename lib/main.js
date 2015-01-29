var atomApp = require('app');

var scope;  // Our only global!

atomApp.on('will-finish-launching', function() {
    scope = createApp();
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
    var rootScope = require('./rootScope');
    var plugins = require('./plugins');

    var scope = rootScope();
    var app = scope.app = express();
    var data = scope.data = symmetry.scope();

    require('./svc/config')(scope);

    require('./model/audioSources')(scope);
    require('./model/videoSources')(scope);
    require('./model/scenes')(scope);
    require('./model/mixers')(scope);

    require('./ctrl/data')(scope);
    require('./ctrl/mixer')(scope);

    var docroot = path.join(__dirname, '..', 'web');
    app.use(express.static(docroot));

    data.p = {};  // Plugin namespaces.
    plugins.pluginNames().forEach(function(pluginName) {
        plugins.loadPlugin(pluginName)(scope);
    });

    return scope;
}
