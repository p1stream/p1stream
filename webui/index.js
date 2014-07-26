var path = require('path');
var express = require('express');

module.exports = function(app) {
    var docroot = path.join(__dirname, 'web');
    app.use(express.static(docroot));
};
