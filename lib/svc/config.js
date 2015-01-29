var fs = require('fs');
var userPaths = require('../userPaths');

module.exports = function(scope) {
    scope.cfgFile = userPaths.configPath();

    try {
        scope.cfg = JSON.parse(fs.readFileSync(scope.cfgFile, 'utf8'));
    }
    catch (err) {
        if (err.code !== 'ENOENT') {
            // FIXME: Proper log
            console.error("Could not read '%s'", scope.cfgFile);
            console.error(err.message);
        }
        scope.cfg = {};
    }

    scope.save = function() {
        scope.$broadcast('saveConfig');
        try {
            fs.writeFileSync(scope.cfgFile, JSON.stringify(this, function(key, value) {
                return key[0] === '$' ? undefined : value;
            }, 2));
        }
        catch (err) {
            // FIXME: Proper log
            console.error("Could not write '%s'", scope.cfgFile);
            console.error(err.message);
        }
    };
};
