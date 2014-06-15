var native = require('./api.node');


function id(v) {
    var b = new Buffer(4);
    b.writeUInt32BE(v, 0);
    if (v <= 0xFF)
        return b.slice(3, 4);
    else if (v <= 0xFFFF)
        return b.slice(2, 4);
    else if (v <= 0xFFFFFF)
        return b.slice(1, 4);
    else if (v <= 0xFFFFFFFF)
        return b.slice(0, 4);
    else
        throw new Error('Invalid ID');
}

var ID_EBML = id(0x1A45DFA3);
var ID_EBMLVersion = id(0x4286);
var ID_EBMLReadVersion = id(0x42F7);
var ID_EBMLMaxIDLength = id(0x42F2);
var ID_EBMLMaxSizeLength = id(0x42F3);
var ID_DocType = id(0x4282);
var ID_DocTypeVersion = id(0x4287);
var ID_DocTypeReadVersion = id(0x4285);
var ID_Segment = id(0x18538067);
var ID_Info = id(0x1549A966);
var ID_TimecodeScale = id(0x2AD7B1);
var ID_MuxingApp = id(0x4D80);
var ID_WritingApp = id(0x5741);
var ID_Tracks = id(0x1654AE6B);
var ID_TrackEntry = id(0xAE);
var ID_TrackNumber = id(0xD7);
var ID_TrackUID = id(0x73C5);
var ID_TrackType = id(0x83);
var ID_FlagEnabled = id(0xB9);
var ID_FlagDefault = id(0x88);
var ID_FlagForced = id(0x55AA);
var ID_FlagLacing = id(0x9C);
var ID_MinCache = id(0x6DE7);
var ID_MaxBlockAdditionID = id(0x55EE);
var ID_CodecID = id(0x86);
var ID_CodecPrivate = id(0x63A2);
var ID_CodecDecodeAll = id(0xAA);
var ID_SeekPreRoll = id(0x56BB);
var ID_Video = id(0xE0);
var ID_FlagInterlaced = id(0x9A);
var ID_PixelWidth = id(0xB0);
var ID_PixelHeight = id(0xBA);
var ID_Cluster = id(0x1F43B675);
var ID_Timecode = id(0xE7);
var ID_SimpleBlock = id(0xA3);


var varInt = native.varInt;

function writeElementHead(id, size, arr) {
    var b = varInt(size, 8);
    arr.push(id);
    arr.push(b);
}

function writeBinary(id, buf, arr) {
    writeElementHead(id, buf.length, arr);
    arr.push(buf);
}

function writeUInt8(id, num, arr) {
    var buf = new Buffer(1);
    buf.writeUInt8(num, 0);
    writeBinary(id, buf, arr);
}

function writeUInt16(id, num, arr) {
    var buf = new Buffer(2);
    buf.writeUInt16BE(num, 0);
    writeBinary(id, buf, arr);
}

function writeUInt32(id, num, arr) {
    var buf = new Buffer(4);
    buf.writeUInt32BE(num, 0);
    writeBinary(id, buf, arr);
}

function writeUInt64(id, num, arr) {
    var buf = new Buffer(8);
    buf.writeUInt32BE(~~(num / 0x0000000100000000), 0);
    buf.writeUInt32BE(num % 0x00000000FFFFFFFF, 4);
    writeBinary(id, buf, arr);
}

function writeString(id, val, arr) {
    var buf = new Buffer(val);
    writeBinary(id, buf, arr);
}

function writeContainer(id, children, arr) {
    var buf = Buffer.concat(children);
    writeBinary(id, buf, arr);
}


exports.headers = function(frame) {
    var list = [];
    var b, l, ll, lll;

    l = []; // EBML
    writeUInt8(ID_EBMLVersion, 1, l);
    writeUInt8(ID_EBMLReadVersion, 1, l);
    writeUInt8(ID_EBMLMaxIDLength, 4, l);
    writeUInt8(ID_EBMLMaxSizeLength, 8, l);
    writeString(ID_DocType, 'matroska', l);
    writeUInt8(ID_DocTypeVersion, 1, l);
    writeUInt8(ID_DocTypeReadVersion, 1, l);
    writeContainer(ID_EBML, l, list);

    writeElementHead(ID_Segment, -1, list);

    l = []; // Info
    writeUInt32(ID_TimecodeScale, 1, l);
    writeString(ID_MuxingApp, 'P1stream', l);
    writeString(ID_WritingApp, 'P1stream', l);
    writeContainer(ID_Info, l, list);

    l = []; // Tracks

    ll = []; // TrackEntry
    writeUInt8(ID_TrackNumber, 1, ll);
    writeUInt8(ID_TrackUID, 1, ll);
    writeUInt8(ID_TrackType, 0x1, ll);
    writeUInt8(ID_FlagEnabled, 1, ll);
    writeUInt8(ID_FlagDefault, 1, ll);
    writeUInt8(ID_FlagForced, 1, ll);
    writeUInt8(ID_FlagLacing, 0, ll);
    writeUInt8(ID_MinCache, 0, ll);
    writeUInt8(ID_MaxBlockAdditionID, 0, ll);
    writeString(ID_CodecID, 'V_MPEG4/ISO/AVC', ll);
    writeBinary(ID_CodecPrivate, codecPrivate(frame), ll);
    writeUInt8(ID_CodecDecodeAll, 1, ll);
    writeUInt8(ID_SeekPreRoll, 0, ll);

    lll = []; // Video
    writeUInt8(ID_FlagInterlaced, 0, lll);
    writeUInt16(ID_PixelWidth, 1280, lll);
    writeUInt16(ID_PixelHeight, 720, lll);
    writeContainer(ID_Video, lll, ll);

    writeContainer(ID_TrackEntry, ll, l);

    writeContainer(ID_Tracks, l, list);

    return Buffer.concat(list);
};

function codecPrivate(frame) {
    var list = [];
    var b;

    var sps = frame.nals.filter(function(nal) {
        return nal.type === 7;
    });
    var pps = frame.nals.filter(function(nal) {
        return nal.type === 8;
    });

    b = new Buffer(6);
    b.writeUInt8(1, 0); // version
    b.writeUInt8(sps[0].buf[5], 1); // profile
    b.writeUInt8(sps[0].buf[6], 2); // profile compat
    b.writeUInt8(sps[0].buf[7], 3); // level
    b.writeUInt8(0xFF, 4); // 6: reserved, 2: Size of NALU lengths, minus 1
    b.writeUInt8(0xE0 | sps.length, 5); // 3: reserved, 5: num SPS
    list.push(b);

    sps.forEach(function(nal) {
        b = new Buffer(2);
        b.writeUInt16BE(nal.buf.length - 4, 0);
        list.push(b);
        list.push(nal.buf.slice(4));
    });

    b = new Buffer(1);
    b.writeUInt8(pps.length, 0);
    list.push(b);

    pps.forEach(function(nal) {
        b = new Buffer(2);
        b.writeUInt16BE(nal.buf.length - 4, 0);
        list.push(b);
        list.push(nal.buf.slice(4));
    });

    return Buffer.concat(list);
}

exports.frame = function(frame) {
    var list = [];
    var b, l, ll, lll;

    l = []; // Cluster
    writeUInt64(ID_Timecode, frame.dts, l);

    ll = []; // SimpleBlock

    b = varInt(1, 1); // Track number
    ll.push(b);

    b = new Buffer(3);
    b.writeInt16BE(0, 0); // Timecode
    b.writeUInt8(frame.keyframe ? 0x01 : 0x00, 2); // Flags
    ll.push(b);

    // Data
    frame.nals.forEach(function(nal) {
        b = new Buffer(4);
        b.writeUInt32BE(nal.buf.length - 4, 0);
        ll.push(b);
        ll.push(nal.buf.slice(4));
    });

    writeBinary(ID_SimpleBlock, Buffer.concat(ll), l);

    writeContainer(ID_Cluster, l, list);

    return Buffer.concat(list);
};
