var events = require('events');
var express = require('express');
var symmetry = require('symmetry');

var config = require('./config');
var matroska = require('./matroska');
var eventSource = require('./eventSource');

var Audio = require('./audio');
var Video = require('./video');

module.exports = function() {
    var app = express();
    app.cfg = config();
    app.data = symmetry.scope({
        streams: Object.create(null)
    });

    // Wrap digest, emit update events.
    app.data.$origDigest = app.data.$digest;
    app.data.$digest = function() {
        var patch = this.$origDigest();
        if (patch !== 'none')
            app.emit('update', patch);
        return patch;
    };

    // Retrieve data.
    app.get('/data', function(req, res, next) {
        res.send(app.data.$last);
    });

    // Retrieve and monitor data, using server-sent events.
    app.get('/data.sse', function(req, res, next) {
        var source = eventSource(res);
        source.send('reset', app.data.$last);

        function onUpdate(patch) {
            if (patch === 'reset')
                source.send('reset', app.data.$last);
            else
                source.send('patch', patch);
        }

        app.addListener('update', onUpdate);
        source.on('close', function() {
            app.removeListener('update', onUpdate);
        });
    });

    // Create a new stream.
    app.post('/streams/new/:id', function(req, res, next) {
        var video, audio, mstream;
        var stream = app.data.streams[req.params.id] = {
            $video: video = new Video(),
            $audio: audio = new Audio(),
            $mstream: mstream = new events.EventEmitter(),
            $headers: null
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
    app.get('/streams/:id.mkv', function(req, res, next) {
        var stream = app.data.streams[req.params.id];
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

    return app;
};
