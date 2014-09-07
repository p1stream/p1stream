var os = require('os');
var fs = require('fs');
var path = require('path');

exports = module.exports = function(file) {
    if (!file) file = exports.defaultPath();

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

exports.defaultPath = function() {
    switch (os.platform()) {
        case 'darwin':
            return path.join(process.env.HOME,
                'Library', 'Preferences', 'nl.stephank.p1stream.json');
        default:
            return path.join(process.env.HOME,
                '.config', 'p1stream.json');
    }
};
