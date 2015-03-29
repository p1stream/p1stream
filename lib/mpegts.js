// MPEG2-TS stream builder library

// Helper: pad a packet using an adaptation field. Takes a buffer, an index in
// the buffer at the start of a packet, and the number of padding bytes.
// Returns a new index into the buffer after the adaptation field. The packet
// header should already be filled before this function.
function padPacket(b, i, num) {
    if (num) {
        if (b[i+3] & 0x20) {
            var existing = b[i+4];  // Assume at least 1
            if (num - 1 > existing) {
                b[i+4] = num;
                b.fill(0xFF, i + 5 + existing, i + 4 + num);
            }
        }
        else {
            b[i+3] |= 0x20;
            b[i+4] = num - 1;
            if (num > 1) {
                b[i+5] = 0x00;
                if (num > 2)
                    b.fill(0xFF, i + 6, i + 4 + num);
            }
        }
    }
    return i + 4 + (b[i+3] & 0x20 ? b[i+4] + 1 : 0);
}

module.exports = function(mixer, dataCb) {
    var lastPcr = 0;
    var seqs = Object.create(null);

    var destroy = mixer.addFrameListener({
        audioFrame: onAudioFrame,
        videoFrame: onVideoFrame
    });

    function onAudioFrame(frame) {
        dataCb(buildAudioFrame(frame));
    }

    function onVideoFrame(frame) {
        if (frame.keyframe) {
            dataCb(buildHeaders());
            dataCb(buildVideoFrame(mixer._videoHeaders));
        }
        dataCb(buildVideoFrame(frame));
    }

    function seq(pid) {
        // Increment seq
        var val = seqs[pid] || 0;
        seqs[pid] = (val + 1) & 0x0F;
        return val;
    }

    function buildHeaders() {
        var b = new Buffer(564);
        var i = 0;

        // Packet header
        b[i+0] = 0x47;  // Sync
        b[i+1] = 0x40;  // Start indicator
        b[i+2] = 0x00;  // PAT PID
        b[i+3] = 0x10 | seq(0x00);  // Have payload

        // Table header
        b[i+4] = 0x00;
        b[i+5] = 0x00;  // PAT table ID
        b[i+6] = 0xB0;
        b[i+7] = 13;  // Length

        b[i+8] = 0x00;

        // Table syntax header
        b[i+ 9] = 0x00;  // Stream ID
        b[i+10] = 0xC1;
        b[i+11] = 0x00;
        b[i+12] = 0x00;

        // Single program
        b[i+13] = 0x00;
        b[i+14] = 0x21;  // Program number
        b[i+15] = 0xE0;
        b[i+16] = 0x21;  // PMT PID

        // CRC and pad
        b[i+17] = 0xC6;
        b[i+18] = 0xB6;
        b[i+19] = 0x78;
        b[i+20] = 0xDc;
        b.fill(0xFF, i + 21, i + 188);
        i += 188;

        // Packet header
        b[i+0] = 0x47;  // Sync
        b[i+1] = 0x40;  // Start indicator
        b[i+2] = 0x21;  // PMT PID
        b[i+3] = 0x10 | seq(0x21);  // Have payload

        // Table header
        b[i+4] = 0x00;
        b[i+5] = 0x02;  // PMT table ID
        b[i+6] = 0xB0;
        b[i+7] = 23;  // Length

        b[i+8] = 0x00;

        // Table syntax header
        b[i+ 9] = 0x21;  // Program number
        b[i+10] = 0xC1;
        b[i+11] = 0x00;
        b[i+12] = 0x00;

        // Program map header
        b[i+13] = 0xE0;
        b[i+14] = 0x22;  // PCR in audio
        b[i+15] = 0xF0;
        b[i+16] = 0x00;  // Length

        // Audio stream info
        b[i+17] = 0x0F;  // AAC
        b[i+18] = 0xE0;
        b[i+19] = 0x22;  // PID
        b[i+20] = 0xF0;
        b[i+21] = 0x00;  // Length

        // Video stream info
        b[i+22] = 0x1B;  // H.264
        b[i+23] = 0xE0;
        b[i+24] = 0x23;  // PID
        b[i+25] = 0xF0;
        b[i+26] = 0x00;  // Length

        // CRC and pad
        b[i+27] = 0xCD;
        b[i+28] = 0x99;
        b[i+29] = 0x80;
        b[i+30] = 0x0B;
        b.fill(0xFF, i + 31, i + 188);

        return b;
    }

    function buildVideoFrame(frame) {
        // Rewrite to Annex B
        var b = new Buffer(frame.buf);
        var off = 0;
        frame.nals.forEach(function(nal) {
            b.writeUInt32BE(1, off);
            off += nal.length;
        });

        return packFrame(b, 0x23, 0xE0, frame.dts, 0);
    }

    function buildAudioFrame(frame) {
        // ADTS header
        var length = 7 + frame.buf.length;
        var b = new Buffer(length);
        b[0] = 0xFF;
        b[1] = 0xF1;  // MPEG-4, no CRC
        b[2] = 0x50;  // AAC LC, 44.1kHz
        b[3] = 0x80 | ((length & 0x1800) >> 11);  // Stereo
        b[4] = (length & 0x07F8) >> 3;
        b[5] = ((length & 0x0007) << 5) | 0x1F;
        b[6] = 0xFC;  // 1 frame
        frame.buf.copy(b, 7);

        // Transmit PCR
        var pcr = 0;
        var pcrDiff = frame.pts - lastPcr;
        if (!lastPcr || frame.pts - lastPcr > 25000000)
            pcr = lastPcr = frame.pts;

        return packFrame(b, 0x22, 0xC0, frame.pts, pcr);
    }

    function packFrame(inb, pid, streamId, pts, pcr) {
        var bytes = inb.length;

        // How much space per packet we have for the payload
        var chunkSize = 184;
        var firstChunkSize = chunkSize - (pts ? 14 : 9) - (pcr ? 8 : 0);

        // Allocate in one shot
        var numPackets = 1;
        if (bytes > firstChunkSize)
            numPackets = 1 + Math.ceil((bytes - firstChunkSize) / chunkSize);
        var b = new Buffer(numPackets * 188);

        // Until we exhaust the input buffer
        var inpos = 0;
        var i = 0;
        while (inpos < bytes) {
            // Calculate payload length
            var available = (inpos ? chunkSize : firstChunkSize);
            var inend = inpos + available;
            if (inend > bytes)
                inend = bytes;
            var dlen = inend - inpos;

            // Packet header
            b[i+0] = 0x47;  // Sync
            b[i+1] = inpos ? 0x00 : 0x40;  // Start indicator
            b[i+2] = pid;  // PID
            b[i+3] = 0x10 | seq(pid);  // Have payload
            if (!inpos && pcr) {
                b[i+3] |= 0x20;  // Have adaptation
                b[i+4] = 7;  // Length
                b[i+5] = 0x10;  // PCR present

                // PCR
                pcr = ~~(pcr * 0.00009);  // As 90kHz
                var low = pcr % 300;
                var high = (pcr - low) / 300;
                b[i+ 6] =  (high >> 25) & 0xFF;
                b[i+ 7] =  (high >> 17) & 0xFF;
                b[i+ 8] =  (high >>  9) & 0xFF;
                b[i+ 9] =  (high >>  1) & 0xFF;
                b[i+10] = ((high <<  7) & 0x80) | ((low >> 8) & 0x01);
                b[i+11] =                           low       & 0xFF;
            }
            i = padPacket(b, i, available - dlen);

            // PES header
            if (!inpos) {
                b[i+0] = 0x00;  // Start code
                b[i+1] = 0x00;
                b[i+2] = 0x01;
                b[i+3] = streamId;
                var pesLength = bytes + (pts ? 8 : 3);
                b.writeUInt16BE(pesLength > 0xFFFF ? 0 : pesLength, i+4);

                b[i+6] = 0x80;
                if (pts) {
                    b[i+7] = 0x80;  // PTS present
                    b[i+8] = 0x05;  // Length

                    // PTS
                    pts = ~~(pts * 0.00009);  // As 90kHz
                    b[i+ 9] = ((pts >> 29) & 0x0E) | 0x21;
                    b[i+10] = ((pts >> 22) & 0xFF);
                    b[i+11] = ((pts >> 14) & 0xFE) | 0x01;
                    b[i+12] = ((pts >>  7) & 0xFF);
                    b[i+13] = ((pts <<  1) & 0xFE) | 0x01;

                    i += 14;
                }
                else {
                    b[i+7] = 0x00;
                    b[i+8] = 0x00;

                    i += 9;
                }
            }

            // Payload
            inb.copy(b, i, inpos, inend);

            i += dlen;
            inpos = inend;
        }

        return b;
    }

    return destroy;
};
