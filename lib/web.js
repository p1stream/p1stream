var path = require('path');
var express = require('express');
var config = require('./config');
var symmetry = require('symmetry');
var rootScope = require('./rootScope');

module.exports = function() {
    var scope = rootScope();
    var app = scope.app = express();
    var cfg = scope.cfg = config.loadConfig();
    var data = scope.data = symmetry.scope();

    scope.$post(function() {
        var digest = data.$digest();
        if (digest !== 'none')
            app.emit('digest', digest);
    });

    data.audioSources = [];
    data.videoSources = [];
    data.scenes = [];
    data.streams = [];
    data.p = {};  // Plugin namespaces.

    require('./ctrl/data')(scope);
    require('./ctrl/stream')(scope);

    var docroot = path.join(__dirname, '..', 'web');
    app.use(express.static(docroot));

    config.pluginNames().forEach(function(pluginName) {
        config.loadPlugin(pluginName)(scope);
    });

    return scope;
};
