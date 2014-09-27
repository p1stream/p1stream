var path = require('path');
var express = require('express');
var config = require('./config');

module.exports = function() {
    var app = express();
    app.cfg = config.loadConfig();
    app.data = require('./dataScope')(function(digest) {
        app.emit('digest', digest);
    });

    app.data.audioSources = [];
    app.data.videoSources = [];
    app.data.scenes = [];
    app.data.streams = [];
    app.data.p = {};  // Plugin namespaces.

    require('./ctrl/data')(app);
    require('./ctrl/stream')(app);

    var docroot = path.join(__dirname, '..', 'web');
    app.use(express.static(docroot));

    config.pluginNames().forEach(function(pluginName) {
        config.loadPlugin(pluginName)(app);
    });

    return app;
};
