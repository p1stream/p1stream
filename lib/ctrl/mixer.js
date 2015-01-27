module.exports = function(scope) {
    var events = require('events');
    var matroska = require('../matroska');

    // Running preview muxers.
    var previews = Object.create(null);
    function createPreview(mixer) {
        var preview = new events.EventEmitter();
        preview.refs = 0;
        preview.headers = null;

        preview.destroy = mixer.addFrameListener({
            audioHeaders: onHeaders,
            videoHeaders: onHeaders,
            audioFrame: onAudioFrame,
            videoFrame: onVideoFrame
        }, { emitInitHeaders: true });

        function onHeaders() {
            if (mixer.$videoHeaders && mixer.$audioHeaders)
                preview.headers = matroska.headers(mixer.$videoHeaders, mixer.$audioHeaders);
        }

        function onAudioFrame(frame) {
            preview.emit('frame', matroska.audioFrame(frame));
        }

        function onVideoFrame(frame) {
            preview.emit('frame', matroska.videoFrame(frame), frame.keyframe);
        }
    }

    // Open a mixer preview as matroska,
    scope.app.get('/api/mixer/:id.mkv', function(req, res, next) {
        var mixerId = req.params.id;

        var mixer = scope.data.mixers[mixerId];
        if (!mixer)
            return res.send(404);

        var preview = previews[mixerId];
        if (!preview)
            preview = previews[mixerId] = createPreview();

        preview.addListener('frame', onFrame);
        preview.consumers++;

        res.on('close', function() {
            preview.removeListener('frame', onFrame);
            preview.consumers--;

            if (preview.consumers === 0) {
                preview.destroy();
                delete previews[mixerId];
            }
        });

        function onFrame(frame, keyframe) {
            if (!preview.headers) return;

            if (!res.headersSent) {
                if (!keyframe) return;

                res.useChunkedEncodingByDefault = false;
                res.writeHead(200, {
                    'Connection': 'close',
                    'Content-Type': 'video/x-matroska'
                });
                res.write(preview.headers);
            }

            res.write(frame);
        }
    });
};
