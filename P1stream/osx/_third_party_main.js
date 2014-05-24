var path = require('path');
var Module = require('module');

var mainPath = path.resolve(process.execPath, '../../Resources/lib/index.js');
Module._load(mainPath, null, true);
