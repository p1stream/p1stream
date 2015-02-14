// Matroska stream builder library.

var writeEBML = require('write-ebml');
var tag = writeEBML.tag;

var T = Object.create(writeEBML.standardTags);
T.Segment = tag(0x18538067, 'm');
T.Info = tag(0x1549A966, 'm');
T.TimecodeScale = tag(0x2AD7B1, 'u');
T.MuxingApp = tag(0x4D80, '8');
T.WritingApp = tag(0x5741, '8');
T.Tracks = tag(0x1654AE6B, 'm');
T.TrackEntry = tag(0xAE, 'm');
T.TrackNumber = tag(0xD7, 'u');
T.TrackUID = tag(0x73C5, 'u');
T.TrackType = tag(0x83, 'u');
T.FlagEnabled = tag(0xB9, 'u');
T.FlagDefault = tag(0x88, 'u');
T.FlagForced = tag(0x55AA, 'u');
T.FlagLacing = tag(0x9C, 'u');
T.MinCache = tag(0x6DE7, 'u');
T.MaxBlockAdditionID = tag(0x55EE, 'u');
T.CodecID = tag(0x86, 's');
T.CodecPrivate = tag(0x63A2, 'b');
T.CodecDecodeAll = tag(0xAA, 'u');
T.SeekPreRoll = tag(0x56BB, 'u');
T.Video = tag(0xE0, 'm');
T.FlagInterlaced = tag(0x9A, 'u');
T.PixelWidth = tag(0xB0, 'u');
T.PixelHeight = tag(0xBA, 'u');
T.Audio = tag(0xE1, 'm');
T.SamplingFrequency = tag(0xB5, 'f');
T.Channels = tag(0x9F, 'u');
T.Cluster = tag(0x1F43B675, 'm');
T.Timecode = tag(0xE7, 'u');
T.SimpleBlock = tag(0xA3, 'b');

module.exports = function(mixer, dataCb) {
    var headersSent = false;

    var destroy = mixer.$addFrameListener({
        audioFrame: onAudioFrame,
        videoFrame: onVideoFrame
    });

    function onAudioFrame(frame) {
        if (headersSent)
            dataCb(buildAudioFrame(frame));
    }

    function onVideoFrame(frame) {
        if (!headersSent && frame.keyframe && mixer.$audioHeaders) {
            dataCb(buildHeaders(mixer.$videoHeaders, mixer.$audioHeaders));
            headersSent = true;
        }
        if (headersSent)
            dataCb(buildVideoFrame(frame));
    }

    return destroy;
};

function buildHeaders(videoHeader, audioHeader) {
    return writeEBML([
        T.EBML([
            T.EBMLVersion(1),
            T.EBMLReadVersion(1),
            T.EBMLMaxIDLength(4),
            T.EBMLMaxSizeLength(8),
            T.DocType('matroska'),
            T.DocTypeVersion(1),
            T.DocTypeReadVersion(1)
        ]),
        T.Segment([
            T.Info([
                T.TimecodeScale(1),
                T.MuxingApp('P1stream'),
                T.WritingApp('P1stream')
            ]),
            T.Tracks([
                T.TrackEntry([
                    T.TrackNumber(1),
                    T.TrackUID(1),
                    T.TrackType(0x1),
                    T.FlagEnabled(1),
                    T.FlagDefault(1),
                    T.FlagForced(1),
                    T.FlagLacing(0),
                    T.MinCache(0),
                    T.MaxBlockAdditionID(0),
                    T.CodecID('V_MPEG4/ISO/AVC'),
                    T.CodecPrivate(buildVideoPrivate(videoHeader)),
                    T.CodecDecodeAll(1),
                    T.SeekPreRoll(0),
                    T.Video([
                        T.FlagInterlaced(0),
                        T.PixelWidth(1280),
                        T.PixelHeight(720)
                    ])
                ]),
                T.TrackEntry([
                    T.TrackNumber(2),
                    T.TrackUID(2),
                    T.TrackType(0x2),
                    T.FlagEnabled(1),
                    T.FlagDefault(1),
                    T.FlagForced(1),
                    T.FlagLacing(0),
                    T.MinCache(0),
                    T.MaxBlockAdditionID(0),
                    T.CodecID('A_AAC'),
                    T.CodecPrivate(buildAudioPrivate(audioHeader)),
                    T.CodecDecodeAll(1),
                    T.SeekPreRoll(0),
                    T.Audio([
                        T.SamplingFrequency(44100),
                        T.Channels(2)
                    ])
                ])
            ])
        ], true)
    ]);
}

function buildVideoPrivate(videoHeader) {
    var parts = [];
    var b;

    var sps = videoHeader.nals.filter(function(nal) {
        return nal.type === 7;
    });
    var pps = videoHeader.nals.filter(function(nal) {
        return nal.type === 8;
    });

    b = new Buffer(6);
    b.writeUInt8(1, 0); // version
    b.writeUInt8(sps[0].buf[5], 1); // profile
    b.writeUInt8(sps[0].buf[6], 2); // profile compat
    b.writeUInt8(sps[0].buf[7], 3); // level
    b.writeUInt8(0xFF, 4); // 6: reserved, 2: Size of NALU lengths, minus 1
    b.writeUInt8(0xE0 | sps.length, 5); // 3: reserved, 5: num SPS
    parts.push(b);

    sps.forEach(function(nal) {
        b = new Buffer(2);
        b.writeUInt16BE(nal.buf.length - 4, 0);
        parts.push(b);
        parts.push(nal.buf.slice(4));
    });

    b = new Buffer(1);
    b.writeUInt8(pps.length, 0);
    parts.push(b);

    pps.forEach(function(nal) {
        b = new Buffer(2);
        b.writeUInt16BE(nal.buf.length - 4, 0);
        parts.push(b);
        parts.push(nal.buf.slice(4));
    });

    return Buffer.concat(parts);
}

function buildAudioPrivate(audioHeader) {
    return audioHeader.buf;
}

function buildVideoFrame(frame) {
    var b;
    var block = [];

    b = new Buffer(4);
    b.writeUInt8(1 | 0x80, 0);  // Track number as varint
    b.writeInt16BE(0, 1);  // Timecode
    b.writeUInt8(frame.keyframe ? 0x01 : 0x00, 3);  // Flags
    block.push(b);

    frame.nals.forEach(function(nal) {
        b = new Buffer(4);
        b.writeUInt32BE(nal.buf.length - 4, 0);
        block.push(b);
        block.push(nal.buf.slice(4));
    });

    return writeEBML([
        T.Cluster([
            T.Timecode(frame.dts),
            T.SimpleBlock(Buffer.concat(block))
        ])
    ]);
}

function buildAudioFrame(frame) {
    var b;
    var block = [];

    b = new Buffer(4);
    b.writeUInt8(2 | 0x80, 0);  // Track number as varint
    b.writeInt16BE(0, 1);  // Timecode
    b.writeUInt8(0x00, 3);  // Flags
    block.push(b);

    block.push(frame.buf);

    return writeEBML([
        T.Cluster([
            T.Timecode(frame.pts),
            T.SimpleBlock(Buffer.concat(block))
        ])
    ]);
}
