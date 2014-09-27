var atomApp = require('app');

var webApp;  // Our only global!

atomApp.on('will-finish-launching', function() {
    webApp = require('./web')();
});

atomApp.on('ready', function() {
    var BrowserWindow = require('browser-window');
    var server = webApp.listen(53311, '127.0.0.1', function() {
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
