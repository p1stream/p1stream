module.exports = function(app) {
    function stream(mimeType, muxerFactory) {
        return function(req, res, next) {
            if (!req.obj)
                return res.status(404).end();

            res.useChunkedEncodingByDefault = false;
            res.set('Connection', 'close');
            res.set('Content-Type', mimeType);

            var destroy = muxerFactory(req.obj, function(data) {
                res.write(data);
            });
            res.on('close', function() {
                destroy();
            });
        };
    }

    // Continuous Matroska stream.
    app.get('/api/mixers/:id.mkv',
        app.resolveParam('id', 'mixer'),
        stream('video/x-matroska', require('../matroska'))
    );

    // Continuous MPEG2-TS stream.
    app.get('/api/mixers/:id.ts',
        app.resolveParam('id', 'mixer'),
        stream('video/MP2T', require('../mpegts'))
    );

    // HLS playlist file.
    app.get('/api/mixers/:id.m3u8',
        app.resolveParam('id', 'mixer'),
        function(req, res, next) {
            if (!req.obj)
                return res.send(404);

            res.set('Content-Type', 'application/x-mpegURL').end(
                '#EXTM3U\n' +
                '#EXT-X-TARGETDURATION:10\n' +
                '#EXT-X-VERSION:3\n' +
                '#EXT-X-MEDIA-SEQUENCE:0\n' +
                '#EXTINF:10.0,\n' +
                req.obj.id + '.ts\n'
            );
        }
    );
};
