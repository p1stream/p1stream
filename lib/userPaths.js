var path = require('path');
var fs = require('fs');

// Configuration file path.
exports.configPath = function() {
    switch (process.platform) {
        case 'darwin':
            return path.join(process.env.HOME,
                'Library', 'Preferences', 'com.p1stream.P1stream.json');
        default:
            var cfgHome = process.env.XDG_CONFIG_HOME ||
                path.join(process.env.HOME, '.config');
            return path.join(cfgHome, 'p1stream.json');
    }
};

// Path to our writeable log directory.
exports.logPath = function() {
    switch (process.platform) {
        case 'darwin':
            return path.join(process.env.HOME,
                'Library', 'Logs', 'P1stream');
        default:
            var dataHome = process.env.XDG_DATA_HOME ||
                path.join(process.env.HOME, '.local', 'share');
            return path.join(dataHome, 'p1stream');
    }
};

// Path to our writeable data directory.
exports.dataPath = function() {
    switch (process.platform) {
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
    switch (process.platform) {
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
    var subdirName = (process.platform === 'linux') ? 'plugins' : 'Plugins';
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

// Ensure writeable dirs exist.
exports.createWriteableDirsSync = function() {
    var dirs = [
        exports.logPath(),
        exports.dataPath()
    ];
    dirs.forEach(function(dir) {
        try {
            fs.mkdirSync(dir);
        }
        catch (err) {
            if (err.code !== 'EEXIST')
                throw err;
        }
    });
};
