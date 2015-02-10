module.exports = function(scope) {
    function stream(muxerType, mimeType) {
        return function(req, res, next) {
            if (!req.mixer)
                return res.send(404);

            var muxer = scope.getMuxer(muxerType, req.mixer);
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
                        'Content-Type': mimeType
                    });
                    res.write(muxer.headers);
                }

                res.write(frame);
            }
        };
    }

    // Continuous Matroska stream.
    scope.app.get('/api/mixers/:id.mkv',
        scope.resolveParam('id', 'mixer'),
        stream('matroska', 'video/x-matroska')
    );

    // Continuous MPEG2-TS stream.
    scope.app.get('/api/mixers/:id.ts',
        scope.resolveParam('id', 'mixer'),
        stream('mpegts', 'video/MP2T')
    );

    // HLS playlist file.
    scope.app.get('/api/mixers/:id.m3u8',
        scope.resolveParam('id', 'mixer'),
        function(req, res, next) {
            if (!req.mixer)
                return res.send(404);

            res.set('Content-Type', 'application/x-mpegURL').end(
                '#EXTM3U\n' +
                '#EXT-X-TARGETDURATION:10\n' +
                '#EXT-X-VERSION:3\n' +
                '#EXT-X-MEDIA-SEQUENCE:0\n' +
                '#EXTINF:10.0,\n' +
                req.mixer.$id + '.ts\n'
            );
        }
    );
};
