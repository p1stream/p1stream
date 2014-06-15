var events = require('events');
var httpApp = require('./httpApp');
var staticFiles = require('./staticFiles');
var bufferStream = require('./bufferStream');
var matroska = require('./matroska');
var core = require('../core');

exports = module.exports = function(app) {
    var video = new core.Video();

    var headers = null;
    var mstream = new events.EventEmitter();
    video.on('headers', function(frame) {
        headers = matroska.headers(frame);
    });
    video.on('frame', function(frame) {
        mstream.emit('frame', matroska.frame(frame), frame.keyframe);
    });

    app.get('/api/stream.mkv', function(req, res, next) {
        mstream.addListener('frame', onFrame);
        res.on('close', function() {
            mstream.removeListener('frame', onFrame);
        });

        function onFrame(frame, keyframe) {
            if (!res.headersSent) {
                if (!keyframe) return;

                res.useChunkedEncodingByDefault = false;
                res.writeHead(200, {
                    'Connection': 'close',
                    'Content-Type': 'video/x-matroska'
                });
                res.write(headers);
            }

            res.write(frame);
        }
    });
};

exports.createApp = function() {
    var app = httpApp.create();
    exports(app);
    return app;
};

exports.httpApp = httpApp;
exports.staticFiles = staticFiles;
exports.bufferStream = bufferStream;
