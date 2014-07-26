var app = require('app');
var BrowserWindow = require('browser-window');

var webapp = null;
var mainWindow = null;

app.on('will-finish-launching', function() {
    webapp = require('./api').createApp();
    require('./webui')(webapp);
});

app.on('ready', function() {
    var server = webapp.listen(53311, '127.0.0.1', function() {
        mainWindow = new BrowserWindow({ width: 800, height: 600 });
        mainWindow.loadUrl('http://localhost:53311/');
        mainWindow.on('closed', function() {
            mainWindow = null;
        });
    });
});

app.on('window-all-closed', function() {
    if (process.platform !== 'darwin')
        app.quit();
});
