module.exports = function(scope) {
    var events = require('events');
    var matroska = require('../matroska');

    // Running preview muxers.
    var previews = Object.create(null);

    // Start a preview for a mixer.
    function createPreview(obj) {
        var preview = new events.EventEmitter();
        preview.refs = 0;
        preview.headers = null;

        preview.destroy = obj.$addFrameListener({
            audioHeaders: onHeaders,
            videoHeaders: onHeaders,
            audioFrame: onAudioFrame,
            videoFrame: onVideoFrame
        }, { emitInitHeaders: true });

        function onHeaders() {
            if (obj.$videoHeaders && obj.$audioHeaders)
                preview.headers = matroska.headers(obj.$videoHeaders, obj.$audioHeaders);
        }

        function onAudioFrame(frame) {
            preview.emit('frame', matroska.audioFrame(frame));
        }

        function onVideoFrame(frame) {
            preview.emit('frame', matroska.videoFrame(frame), frame.keyframe);
        }

        return preview;
    }

    // Open a mixer preview as matroska,
    scope.app.get('/api/mixers/:id.mkv', function(req, res, next) {
        var id = req.params.id;

        var obj = id[0] !== '$' && scope.o[id];
        if (!obj || obj.cfg.type !== 'mixer')
            return res.send(404);

        var preview = previews[id];
        if (!preview)
            preview = previews[id] = createPreview(obj);

        preview.addListener('frame', onFrame);
        preview.consumers++;

        res.on('close', function() {
            preview.removeListener('frame', onFrame);
            preview.consumers--;

            if (preview.consumers === 0) {
                preview.destroy();
                delete previews[id];
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
