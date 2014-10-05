module.exports = function(scope) {
    var events = require('events');
    var matroska = require('../matroska');
    var Audio = require('../audio');
    var Video = require('../video');

    // Create a new stream.
    // FIXME: proper model layer
    scope.app.post('/api/streams/new/:id', function(req, res, next) {
        var video, audio, mstream;
        var stream = scope.data.streams[req.params.id] = {};
        Object.defineProperties(stream, {
            $video: video = new Video(),
            $audio: audio = new Audio(),
            $mstream: mstream = new events.EventEmitter(),
            $headers: null
        });

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
                stream.$headers = matroska.headers(videoHeaders, audioHeaders);
        }

        video.on('frame', function(frame) {
            mstream.emit('frame', matroska.videoFrame(frame), frame.keyframe);
        });

        audio.on('frame', function(frame) {
            mstream.emit('frame', matroska.audioFrame(frame));
        });
    });

    // Open stream as matroska,
    scope.app.get('/api/streams/:id.mkv', function(req, res, next) {
        var stream = scope.data.streams[req.params.id];
        if (!stream)
            return res.send(404);

        stream.$mstream.addListener('frame', onFrame);
        res.on('close', function() {
            stream.$mstream.removeListener('frame', onFrame);
        });

        function onFrame(frame, keyframe) {
            if (!stream.$headers) return;

            if (!res.headersSent) {
                if (!keyframe) return;

                res.useChunkedEncodingByDefault = false;
                res.writeHead(200, {
                    'Connection': 'close',
                    'Content-Type': 'video/x-matroska'
                });
                res.write(stream.$headers);
            }

            res.write(frame);
        }
    });
};
