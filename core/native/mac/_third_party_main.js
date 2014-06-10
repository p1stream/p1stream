var path = require('path');
var Module = require('module');

var mainPath = path.resolve(process.execPath, '../../Modules/core/main.js');
Module._load(mainPath, null, true);
