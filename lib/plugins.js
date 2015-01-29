var fs = require('fs');
var path = require('path');
var userPaths = require('./userPaths');

// List detected plugins.
exports.pluginNames = function() {
    var pluginNames = Object.create(null);

    var deps = require('../package.json').pluginDependencies;

    Object.keys(deps.common || {}).forEach(function(pluginName) {
        pluginNames[pluginName] = true;
    });

    Object.keys(deps[process.platform] || {}).forEach(function(pluginName) {
        pluginNames[pluginName] = true;
    });

    userPaths.userPluginPaths().forEach(function(dir) {
        try {
            var pluginName = fs.readdirSync(dir);
            if (pluginName[0] === '.') return;

            var stat = fs.statSync(path.join(dir, pluginName));
            if (!stat.isDirectory()) return;

            pluginNames[pluginName] = true;
        }
        catch (err) {
            if (err.code === 'ENOENT') return;
            throw err;
        }
    });

    return Object.keys(pluginNames);
};

// Load a plugin. Wraps `require()`.
exports.loadPlugin = function(pluginName) {
    var pluginModule;

    var found = userPaths.pluginSearchPaths().some(function(dir) {
        var pluginDir = path.join(dir, pluginName);

        try {
            var stat = fs.statSync(pluginDir);
            if (!stat.isDirectory()) return false;
        }
        catch (err) {
            if (err.code === 'ENOENT') return false;
            throw err;
        }

        pluginModule = require(pluginDir);
        return true;
    });

    if (!found)
        throw new Error("Plugin '" + pluginName + "' not found");

    return pluginModule;
};
