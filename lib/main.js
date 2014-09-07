var atomApp = require('app');

var app;  // Our only global!

atomApp.on('will-finish-launching', function() {
    var path = require('path');
    var express = require('express');
    var config = require('./config');

    app = express();
    app.cfg = config.loadConfig();
    app.scope = require('./dataScope')(function(digest) {
        app.emit('digest', digest);
    });

    app.scope.streams = {};
    app.scope.audioSources = [];
    app.scope.videoSources = [];
    app.scope.p = {};  // Plugin namespaces.

    require('./ctrl/data')(app);
    require('./ctrl/stream')(app);

    var docroot = path.join(__dirname, '..', 'web');
    app.use(express.static(docroot));

    config.pluginNames().forEach(function(pluginName) {
        config.loadPlugin(pluginName)(app);
    });
});

atomApp.on('ready', function() {
    var BrowserWindow = require('browser-window');

    var server = app.listen(53311, '127.0.0.1', function() {
        var mainWindow = new BrowserWindow({ width: 800, height: 600 });
        mainWindow.loadUrl('http://localhost:53311/');
        mainWindow.on('closed', function() {
            mainWindow = null;
        });
    });
});

atomApp.on('window-all-closed', function() {
    if (process.platform !== 'darwin')
        atomApp.quit();
});
