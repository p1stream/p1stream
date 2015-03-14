// Build AVC decoder config from video headers.
exports.config = function(videoHeaders) {
    var parts = [];
    var b;

    var sps = videoHeaders.nals.filter(function(nal) {
        return nal.type === 7;
    });
    var pps = videoHeaders.nals.filter(function(nal) {
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
};
