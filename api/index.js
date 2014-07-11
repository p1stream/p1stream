var events = require('events');
var httpApp = require('./httpApp');
var staticFiles = require('./staticFiles');
var bufferStream = require('./bufferStream');
var matroska = require('./matroska');
var core = require('../core');

exports = module.exports = function(app) {
    var video = new core.Video();
    var audio = new core.Audio();
    var mstream = new events.EventEmitter();

    var videoHeaders = null;
    var audioHeaders = null;
    video.once('headers', function(frame) {
        videoHeaders = frame;
        buildHeaders();
    });
    audio.once('headers', function(frame) {
        audioHeaders = frame;
        buildHeaders();
    });

    var headers = null;
    function buildHeaders() {
        if (videoHeaders && audioHeaders)
            headers = matroska.headers(videoHeaders, audioHeaders);
    }

    video.on('frame', function(frame) {
        mstream.emit('frame', matroska.videoFrame(frame), frame.keyframe);
    });

    audio.on('frame', function(frame) {
        mstream.emit('frame', matroska.audioFrame(frame));
    });

    app.get('/api/stream.mkv', function(req, res, next) {
        mstream.addListener('frame', onFrame);
        res.on('close', function() {
            mstream.removeListener('frame', onFrame);
        });

        function onFrame(frame, keyframe) {
            if (!headers) return;

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
