var events = require('events');
var express = require('express');
var symmetry = require('symmetry');

var config = require('./config');
var matroska = require('./matroska');
var eventSource = require('./eventSource');

var Audio = require('./audio');
var Video = require('./video');

function createDataScope(app) {
    var data = app.data = symmetry.scope();
    var mark = 0;
    var lastMark = 0;
    var watchers = [];

    // Install/uninstall watchers.
    data.$watch = function(fn) {
        var idx = watchers.indexOf(fn);
        if (idx === -1)
            watchers.push(fn);
    };
    data.$unwatch = function(fn) {
        var idx = watchers.indexOf(fn);
        if (idx !== -1)
            watchers.splice(idx, 1);
    };

    // Run watchers, emit an update.
    data.$mark = function() {
        if (mark > lastMark) return;
        mark++;

        if (mark > 10)
            throw new Error("Watcher loop exceeded 10 iterations");

        process.nextTick(function() {
            lastMark = mark;
            watchers.some(function(fn) {
                fn();
                return mark > lastMark;
            });
            if (mark > lastMark) return;

            mark = lastMark = 0;

            var digest = data.$digest();
            if (digest !== 'none')
                app.emit('digest', digest);
        });
    };

    data.$mark();
}

function createDataRoutes(app) {
    // Retrieve data.
    app.get('/data', function(req, res, next) {
        res.send(app.data.$last);
    });

    // Retrieve and monitor data, using server-sent events.
    app.get('/data.sse', function(req, res, next) {
        var source = eventSource(res);
        source.send('reset', app.data.$last);

        function onDigest(digest) {
            if (digest === 'reset')
                source.send('reset', app.data.$last);
            else
                source.send('patch', digest);
        }

        app.addListener('digest', onDigest);
        source.on('close', function() {
            app.removeListener('digest', onDigest);
        });
    });
}

module.exports = function() {
    var app = express();
    app.cfg = config();

    createDataScope(app);
    createDataRoutes(app);

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
