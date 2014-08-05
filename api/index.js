var events = require('events');
var config = require('./config');
var matroska = require('./matroska');
var core = require('../core');

module.exports = function(app) {
    app.cfg = config();

    var streams = Object.create(null);

    app.post('/api/streams/new/:id', function(req, res, next) {
        var video, audio, mstream;
        var stream = streams[req.params.id] = {
            video: video = new core.Video(),
            audio: audio = new core.Audio(),
            mstream: mstream = new events.EventEmitter(),
            headers: null
        };

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

        function buildHeaders() {
            if (videoHeaders && audioHeaders)
                stream.headers = matroska.headers(videoHeaders, audioHeaders);
        }

        video.on('frame', function(frame) {
            mstream.emit('frame', matroska.videoFrame(frame), frame.keyframe);
        });

        audio.on('frame', function(frame) {
            mstream.emit('frame', matroska.audioFrame(frame));
        });
    });

    app.get('/api/streams/:id.mkv', function(req, res, next) {
        var stream = streams[req.params.id];
        if (!stream)
            return res.send(404);

        stream.mstream.addListener('frame', onFrame);
        res.on('close', function() {
            stream.mstream.removeListener('frame', onFrame);
        });

        function onFrame(frame, keyframe) {
            if (!stream.headers) return;

            if (!res.headersSent) {
                if (!keyframe) return;

                res.useChunkedEncodingByDefault = false;
                res.writeHead(200, {
                    'Connection': 'close',
                    'Content-Type': 'video/x-matroska'
                });
                res.write(stream.headers);
            }

            res.write(frame);
        }
    });
};
