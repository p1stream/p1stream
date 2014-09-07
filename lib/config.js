var os = require('os');
var fs = require('fs');
var path = require('path');

// Configuration file path.
exports.configPath = function() {
    switch (os.platform()) {
        case 'darwin':
            return path.join(process.env.HOME,
                'Library', 'Preferences', 'nl.stephank.p1stream.json');
        default:
            var cfgHome = process.env.XDG_CONFIG_HOME ||
                path.join(process.env.HOME, '.config');
            return path.join(cfgHome, 'p1stream.json');
    }
};

// Load the configuration file.
exports.loadConfig = function(file) {
    if (!file) file = exports.configPath();

    var config;
    try {
        config = JSON.parse(fs.readFileSync(file, 'utf8'));
    }
    catch (err) {
        if (err.code !== 'ENOENT') {
            console.error("Could not read '%s'", file);
            console.error(err.message);
        }
        config = {};
    }

    Object.defineProperties(config, {
        $file: { value: file },
        $save: { value: save }
    });

    return config;
};

// $save method on the config object.
function save() {
    var file = this.$file;
    try {
        fs.writeFileSync(file, JSON.stringify(this, null, 4));
    }
    catch (err) {
        console.error("Could not write '%s'", file);
        console.error(err.message);
    }
}

// Path to our writeable data directory.
exports.dataPath = function() {
    switch (os.platform()) {
        case 'darwin':
            return path.join(process.env.HOME,
                'Library', 'Application Support', 'P1stream');
        default:
            var dataHome = process.env.XDG_DATA_HOME ||
                path.join(process.env.HOME, '.local', 'share');
            return path.join(dataHome, 'p1stream');
    }
};

// Paths to data directories.
exports.dataReadPaths = function() {
    switch (os.platform()) {
        case 'darwin':
            return [exports.dataPath()];
        default:
            var dataDirs = process.env.XDG_DATA_DIRS ||
                '/usr/local/share/:/usr/share/';
            dataDirs = dataDirs.split(':').map(function(dataDir) {
                return path.join(dataDir, 'p1stream');
            });
            return [exports.dataPath()].concat(dataDirs);
    }
};

// Paths to user plugin directories.
exports.userPluginPaths = function() {
    var subdirName = (os.platform() === 'linux') ? 'plugins' : 'Plugins';
    return exports.dataReadPaths().map(function(dataDir) {
        return path.join(dataDir, subdirName);
    });
};

// Search paths for plugins.
exports.pluginSearchPaths = function() {
    var bundleDir = path.join(__dirname, '..', 'node_modules');
    var userDirs = exports.userPluginPaths();
    return userDirs.concat([bundleDir]);
};

// List of detected plugins.
exports.pluginNames = function() {
    var pluginNames = require('../package.json').pluginDependencies;

    exports.userPluginPaths().forEach(function(dir) {
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

    var found = exports.pluginSearchPaths().some(function(dir) {
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
