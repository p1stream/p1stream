var native = require('./api.node');


function tag(id, type) {
    var b = new Buffer(4);
    b.writeUInt32BE(id, 0);
    if (id <= 0xFF)
        b = b.slice(3, 4);
    else if (id <= 0xFFFF)
        b = b.slice(2, 4);
    else if (id <= 0xFFFFFF)
        b = b.slice(1, 4);
    else if (id > 0xFFFFFFFF)
        throw new Error('Invalid ID');

    return function(val, sizeUnknown) {
        return [b, type, val, sizeUnknown];
    };
}

var T = {
    EBML: tag(0x1A45DFA3, 'm'),
    EBMLVersion: tag(0x4286, 'u'),
    EBMLReadVersion: tag(0x42F7, 'u'),
    EBMLMaxIDLength: tag(0x42F2, 'u'),
    EBMLMaxSizeLength: tag(0x42F3, 'u'),
    DocType: tag(0x4282, 's'),
    DocTypeVersion: tag(0x4287, 'u'),
    DocTypeReadVersion: tag(0x4285, 'u'),
    Segment: tag(0x18538067, 'm'),
    Info: tag(0x1549A966, 'm'),
    TimecodeScale: tag(0x2AD7B1, 'u'),
    MuxingApp: tag(0x4D80, '8'),
    WritingApp: tag(0x5741, '8'),
    Tracks: tag(0x1654AE6B, 'm'),
    TrackEntry: tag(0xAE, 'm'),
    TrackNumber: tag(0xD7, 'u'),
    TrackUID: tag(0x73C5, 'u'),
    TrackType: tag(0x83, 'u'),
    FlagEnabled: tag(0xB9, 'u'),
    FlagDefault: tag(0x88, 'u'),
    FlagForced: tag(0x55AA, 'u'),
    FlagLacing: tag(0x9C, 'u'),
    MinCache: tag(0x6DE7, 'u'),
    MaxBlockAdditionID: tag(0x55EE, 'u'),
    CodecID: tag(0x86, 's'),
    CodecPrivate: tag(0x63A2, 'b'),
    CodecDecodeAll: tag(0xAA, 'u'),
    SeekPreRoll: tag(0x56BB, 'u'),
    Video: tag(0xE0, 'm'),
    FlagInterlaced: tag(0x9A, 'u'),
    PixelWidth: tag(0xB0, 'u'),
    PixelHeight: tag(0xBA, 'u'),
    Audio: tag(0xE1, 'm'),
    SamplingFrequency: tag(0xB5, 'u'),
    Channels: tag(0x9F, 'u'),
    Cluster: tag(0x1F43B675, 'm'),
    Timecode: tag(0xE7, 'u'),
    SimpleBlock: tag(0xA3, 'b')
};



exports.headers = function(videoHeader, audioHeader) {
    return native.buildEBML([
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
                    T.CodecPrivate(videoPrivate(videoHeader)),
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
                    T.CodecPrivate(audioPrivate(audioHeader)),
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
};

function videoPrivate(videoHeader) {
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

function audioPrivate(audioHeader) {
    return audioHeader.buf;
}

exports.videoFrame = function(frame) {
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

    return native.buildEBML([
        T.Cluster([
            T.Timecode(frame.dts),
            T.SimpleBlock(Buffer.concat(block))
        ])
    ]);
};

exports.audioFrame = function(frame) {
    var b;
    var block = [];

    b = new Buffer(4);
    b.writeUInt8(2 | 0x80, 0);  // Track number as varint
    b.writeInt16BE(0, 1);  // Timecode
    b.writeUInt8(0x00, 3);  // Flags
    block.push(b);

    block.push(frame.buf);

    return native.buildEBML([
        T.Cluster([
            T.Timecode(frame.pts),
            T.SimpleBlock(Buffer.concat(block))
        ])
    ]);
};
