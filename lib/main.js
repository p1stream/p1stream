var atomApp = require('app');
var path = require('path');
var express = require('express');
var BrowserWindow = require('browser-window');

var app, mainWindow;

atomApp.on('will-finish-launching', function() {
    var config = require('./config');

    app = express();

    app.cfg = config.loadConfig();

    app.api = require('./app')();
    app.use('/api', app.api);

    var docroot = path.join(__dirname, '..', 'web');
    app.use(express.static(docroot));

    config.pluginNames().forEach(function(pluginName) {
        config.loadPlugin(pluginName)(app);
    });
});

atomApp.on('ready', function() {
    var server = app.listen(53311, '127.0.0.1', function() {
        mainWindow = new BrowserWindow({ width: 800, height: 600 });
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
