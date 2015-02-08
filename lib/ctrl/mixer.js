module.exports = function(scope) {
    // A continuous Matroska stream.
    scope.app.get('/api/mixers/:id.mkv', function(req, res, next) {
        var id = req.params.id;

        var obj = id[0] !== '$' && scope.o[id];
        if (!obj || obj.cfg.type !== 'mixer')
            return res.send(404);

        var muxer = scope.getMkvMuxer(obj);
        muxer.addListener('frame', onFrame);
        muxer.consumers++;

        res.on('close', function() {
            muxer.removeListener('frame', onFrame);
            muxer.unref();
        });

        function onFrame(frame, keyframe) {
            if (!muxer.headers) return;

            if (!res.headersSent) {
                if (!keyframe) return;

                res.useChunkedEncodingByDefault = false;
                res.writeHead(200, {
                    'Connection': 'close',
                    'Content-Type': 'video/x-matroska'
                });
                res.write(muxer.headers);
            }

            res.write(frame);
        }
    });
};
